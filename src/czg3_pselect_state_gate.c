#include "common.h"
#include "czg3_diag.h"

#if defined(APP_CZG3_PSELECT_STATE_GATE) && APP_CZG3_PSELECT_STATE_GATE

/*
 * CZG3 FOPS pselect gate v2.
 *
 * The first gate used /proc/self/task/<tid>/{syscall,wchan} polling before and
 * after the selected FOPS delay. Device logs showed that pselect6/do_select did
 * not distinguish winning from losing FOPS states and the polling itself moved
 * the action roughly 0.5-0.7 ms later.
 *
 * v2 reuses the existing production RMG_RACE_LIGHT_V1 state that slide_app.c
 * already records with CNTVCT_EL0 + relaxed atomic stores. PSELECT_ENTER is the
 * timing origin and PSELECT_RETURN doubles as the active-state marker. No /proc
 * reads, directory scans, or success-path logging are performed in the hot
 * path. P0 remains outside this interposer because the light race state is only
 * enabled for the FOPS oracle route.
 */

extern int __real_usleep(useconds_t usec);
extern long __real_sched_setattr_tid(int tid, int nice_value);

/* Shared backing for the low-overhead inline telemetry in czg3_diag.h. */
atomic_uint_fast64_t czg3_light_race_ticks[CZG3_LIGHT_SLOT_COUNT];
atomic_int czg3_light_race_attempt;
atomic_int czg3_light_race_enabled;

enum gate_failure {
  GATE_OK = 0,
  GATE_BAD_COUNTER,
  GATE_NO_PSELECT_ENTER,
  GATE_PSELECT_RETURNED,
  GATE_TRIGGER_LATE
};

struct czg3_pselect_gate_ctx {
  int active;
  int ready_ok;
  enum gate_failure failure;
  useconds_t effective_delay_usec;
  uint64_t counter_hz;
  uint64_t pselect_enter_tick;
  uint64_t deadline_tick;
  uint64_t ready_wait_usec;
  uint64_t deadline_late_usec;
};

static _Thread_local struct czg3_pselect_gate_ctx gate_ctx;
static atomic_int gate_banner_printed;

static const char *gate_failure_name(enum gate_failure failure) {
  switch (failure) {
    case GATE_OK:
      return "ok";
    case GATE_BAD_COUNTER:
      return "bad-counter";
    case GATE_NO_PSELECT_ENTER:
      return "no-pselect-enter";
    case GATE_PSELECT_RETURNED:
      return "pselect-returned";
    case GATE_TRIGGER_LATE:
      return "trigger-late";
    default:
      return "unknown";
  }
}

static uint64_t gate_usec_to_ticks(uint64_t usec, uint64_t frequency) {
  if (!frequency || usec > UINT64_MAX / frequency) {
    return 0;
  }
  uint64_t product = usec * frequency;
  return (product + 999999ULL) / 1000000ULL;
}

static uint64_t gate_ticks_to_usec(uint64_t ticks, uint64_t frequency) {
  if (!frequency) {
    return UINT64_MAX;
  }
  return (ticks * 1000000ULL) / frequency;
}

/* Return 1 for CZG3 FOPS with valid delay metadata, 0 for a non-FOPS route,
 * and -1 when the FOPS route is identified but the delay metadata is invalid.
 */
static int gate_get_effective_fops_delay(useconds_t *delay_out) {
  if (slide_oracle_parent != fake_fops ||
      slide_oracle_target != data_addr(ASHMEM_MISC_FOPS)) {
    return 0;
  }

  const char *text = getenv("SLIDE_ENTER_DELAY_USEC");
  if (!text || !*text) {
    return -1;
  }
  char *end = NULL;
  errno = 0;
  unsigned long parsed = strtoul(text, &end, 0);
  if (errno || end == text || *end || parsed > 1000000UL) {
    return -1;
  }
  if (delay_out) {
    *delay_out = (useconds_t)parsed;
  }
  return 1;
}

static int gate_matches_fops_delay(useconds_t usec) {
  useconds_t effective_delay = 0;
  return gate_get_effective_fops_delay(&effective_delay) == 1 &&
         effective_delay == usec;
}

static uint64_t gate_load_tick(enum czg3_light_race_slot slot) {
  return atomic_load_explicit(&czg3_light_race_ticks[slot],
                              memory_order_relaxed);
}

static int gate_wait_for_pselect_enter(uint64_t frequency,
                                       uint64_t *enter_tick,
                                       uint64_t *wait_usec) {
  uint64_t started = czg3_light_read_counter();
  uint64_t timeout_ticks = gate_usec_to_ticks(
      APP_CZG3_PSELECT_START_TIMEOUT_USEC, frequency);
  if (!started || !timeout_ticks) {
    return 0;
  }

  for (;;) {
    uint64_t enter = gate_load_tick(CZG3_LIGHT_PSELECT_ENTER);
    if (enter) {
      if (enter_tick) {
        *enter_tick = enter;
      }
      if (wait_usec) {
        uint64_t now = czg3_light_read_counter();
        *wait_usec = now >= started
            ? gate_ticks_to_usec(now - started, frequency)
            : 0;
      }
      return 1;
    }
    if (gate_load_tick(CZG3_LIGHT_PSELECT_RETURN)) {
      return 0;
    }

    uint64_t now = czg3_light_read_counter();
    if (!now || now < started || now - started >= timeout_ticks) {
      return 0;
    }
    __asm__ volatile("yield" ::: "memory");
  }
}

static int gate_wait_until_deadline(useconds_t delay_usec) {
  memset(&gate_ctx, 0, sizeof(gate_ctx));
  gate_ctx.active = 1;
  gate_ctx.effective_delay_usec = delay_usec;
  gate_ctx.failure = GATE_BAD_COUNTER;
  gate_ctx.counter_hz = czg3_light_read_frequency();
  if (!gate_ctx.counter_hz) {
    return 0;
  }

  if (!gate_wait_for_pselect_enter(gate_ctx.counter_hz,
                                   &gate_ctx.pselect_enter_tick,
                                   &gate_ctx.ready_wait_usec)) {
    gate_ctx.failure = gate_load_tick(CZG3_LIGHT_PSELECT_RETURN)
        ? GATE_PSELECT_RETURNED
        : GATE_NO_PSELECT_ENTER;
    return 0;
  }

  uint64_t delay_ticks = gate_usec_to_ticks(delay_usec, gate_ctx.counter_hz);
  if (delay_usec && !delay_ticks) {
    gate_ctx.failure = GATE_BAD_COUNTER;
    return 0;
  }
  if (UINT64_MAX - gate_ctx.pselect_enter_tick < delay_ticks) {
    gate_ctx.failure = GATE_BAD_COUNTER;
    return 0;
  }
  gate_ctx.deadline_tick = gate_ctx.pselect_enter_tick + delay_ticks;

  for (;;) {
    if (gate_load_tick(CZG3_LIGHT_PSELECT_RETURN)) {
      gate_ctx.failure = GATE_PSELECT_RETURNED;
      return 0;
    }

    uint64_t now = czg3_light_read_counter();
    if (!now) {
      gate_ctx.failure = GATE_BAD_COUNTER;
      return 0;
    }
    if (now >= gate_ctx.deadline_tick) {
      gate_ctx.deadline_late_usec = gate_ticks_to_usec(
          now - gate_ctx.deadline_tick, gate_ctx.counter_hz);
      break;
    }

    uint64_t remaining_usec = gate_ticks_to_usec(
        gate_ctx.deadline_tick - now, gate_ctx.counter_hz);
    if (remaining_usec > 2000) {
      /* Sleep most of the remaining interval, then finish with a short yield
       * loop. This keeps the target anchored to PSELECT_ENTER rather than to
       * the time spent entering this wrapper. */
      useconds_t sleep_usec = (useconds_t)(remaining_usec - 1000);
      if (sleep_usec) {
        __real_usleep(sleep_usec);
      }
    } else {
      __asm__ volatile("yield" ::: "memory");
    }
  }

  if (gate_load_tick(CZG3_LIGHT_PSELECT_RETURN)) {
    gate_ctx.failure = GATE_PSELECT_RETURNED;
    return 0;
  }
  if (gate_ctx.deadline_late_usec > APP_CZG3_PSELECT_LATE_TOLERANCE_USEC) {
    gate_ctx.failure = GATE_TRIGGER_LATE;
    return 0;
  }

  gate_ctx.failure = GATE_OK;
  gate_ctx.ready_ok = 1;
  return 1;
}

int __wrap_usleep(useconds_t usec) {
  if (!gate_matches_fops_delay(usec)) {
    return __real_usleep(usec);
  }

  /* Do not add the original delay after this call. gate_wait_until_deadline()
   * has already waited until PSELECT_ENTER + effective_delay. */
  (void)gate_wait_until_deadline(usec);
  return 0;
}

static long gate_skip_trigger(int tid, const char *reason) {
  uint64_t enter = gate_load_tick(CZG3_LIGHT_PSELECT_ENTER);
  uint64_t returned = gate_load_tick(CZG3_LIGHT_PSELECT_RETURN);
  uint64_t now = czg3_light_read_counter();
  uint64_t age_usec = UINT64_MAX;
  if (enter && now >= enter && gate_ctx.counter_hz) {
    age_usec = gate_ticks_to_usec(now - enter, gate_ctx.counter_hz);
  }
  pr_info("slide fops pselect state gate v2 trigger=skipped reason=%s "
          "gate_failure=%s delay_usec=%u ready_wait_usec=%llu "
          "late_usec=%llu pselect_age_usec=%llu enter=%llu return=%llu "
          "call_tid=%d\n",
          reason, gate_failure_name(gate_ctx.failure),
          (unsigned int)gate_ctx.effective_delay_usec,
          (unsigned long long)gate_ctx.ready_wait_usec,
          (unsigned long long)gate_ctx.deadline_late_usec,
          (unsigned long long)age_usec,
          (unsigned long long)enter,
          (unsigned long long)returned,
          tid);
  memset(&gate_ctx, 0, sizeof(gate_ctx));
  errno = EAGAIN;
  return -1;
}

long __wrap_sched_setattr_tid(int tid, int nice_value) {
  useconds_t effective_delay = 0;
  int fops_route = gate_get_effective_fops_delay(&effective_delay);
  if (fops_route == 0) {
    return __real_sched_setattr_tid(tid, nice_value);
  }
  if (fops_route < 0) {
    memset(&gate_ctx, 0, sizeof(gate_ctx));
    gate_ctx.active = 1;
    gate_ctx.failure = GATE_BAD_COUNTER;
    return gate_skip_trigger(tid, "bad-delay-metadata");
  }

  if (!gate_ctx.active) {
    if (effective_delay != 0) {
      gate_ctx.effective_delay_usec = effective_delay;
      gate_ctx.failure = GATE_NO_PSELECT_ENTER;
      return gate_skip_trigger(tid, "unarmed");
    }
    /* slide_wait_before_consume() intentionally omits usleep(0), so zero-delay
     * uses the same marker-based arm path here. */
    (void)gate_wait_until_deadline(0);
  }

  if (gate_ctx.effective_delay_usec != effective_delay) {
    return gate_skip_trigger(tid, "delay-changed");
  }
  if (!gate_ctx.ready_ok) {
    return gate_skip_trigger(tid, "gate-not-ready");
  }

  uint64_t enter = gate_load_tick(CZG3_LIGHT_PSELECT_ENTER);
  uint64_t returned = gate_load_tick(CZG3_LIGHT_PSELECT_RETURN);
  uint64_t now = czg3_light_read_counter();
  if (!enter || enter != gate_ctx.pselect_enter_tick) {
    gate_ctx.failure = GATE_NO_PSELECT_ENTER;
    return gate_skip_trigger(tid, "enter-marker-changed");
  }
  if (returned) {
    gate_ctx.failure = GATE_PSELECT_RETURNED;
    return gate_skip_trigger(tid, "pselect-returned");
  }
  if (!now || now < gate_ctx.deadline_tick) {
    gate_ctx.failure = GATE_BAD_COUNTER;
    return gate_skip_trigger(tid, "deadline-not-reached");
  }

  uint64_t late_usec = gate_ticks_to_usec(
      now - gate_ctx.deadline_tick, gate_ctx.counter_hz);
  if (late_usec > APP_CZG3_PSELECT_LATE_TOLERANCE_USEC) {
    gate_ctx.deadline_late_usec = late_usec;
    gate_ctx.failure = GATE_TRIGGER_LATE;
    return gate_skip_trigger(tid, "trigger-late");
  }

  /* No success-path logging. At this point the hot-path work added by v2 is
   * only atomic marker loads plus CNTVCT reads; the existing telemetry records
   * the real sched_setattr completion afterward. */
  memset(&gate_ctx, 0, sizeof(gate_ctx));
  return __real_sched_setattr_tid(tid, nice_value);
}

__attribute__((constructor)) static void czg3_state_gate_banner(void) {
  if (atomic_exchange(&gate_banner_printed, 1) == 0) {
    pr_info("CZG3 FOPS pselect state gate v2 enabled start_timeout_us=%d "
            "late_tolerance_us=%d default_fops_delay_us=%d "
            "source=RMG_RACE_LIGHT_V1\n",
            APP_CZG3_PSELECT_START_TIMEOUT_USEC,
            APP_CZG3_PSELECT_LATE_TOLERANCE_USEC,
            APP_FOPS_ROUTE_COARSE_DELAY_USEC);
  }
}

#endif
