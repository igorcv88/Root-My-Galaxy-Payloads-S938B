#define _GNU_SOURCE
#include "czg3_diag.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#if defined(CZG3_RACE_TELEMETRY) && CZG3_RACE_TELEMETRY
#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#endif

static uint64_t run_id;
static uint64_t started_ns;
static const char *profile_name = "unset";
static pid_t diag_owner_pid;

static uint64_t now_ns(clockid_t clock) {
  struct timespec ts = {0};
  return clock_gettime(clock, &ts) == 0
             ? (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec
             : 0;
}

const char *czg3_failure_name(enum czg3_failure failure) {
  static const char *const names[] = {
      "PRECONDITION_FAILED", "P0_DISCOVERY_FAILED", "P0_STATE_INVALID",
      "RACE_NOT_WON", "RACE_TIMEOUT_CLEAN", "RACE_STATE_UNCERTAIN",
      "CLEANUP_FAILED", "PRIMITIVE_VALIDATION_FAILED",
      "PRIVILEGE_BOOTSTRAP_FAILED", "SUCCESS"};
  return (unsigned)failure < sizeof(names) / sizeof(names[0])
             ? names[failure]
             : "RACE_STATE_UNCERTAIN";
}

enum czg3_retry_safety czg3_retry_policy(enum czg3_failure failure,
                                          int cleanup_complete) {
  if (!cleanup_complete)
    return CZG3_UNSAFE_OR_UNKNOWN;
  return failure == CZG3_RACE_NOT_WON || failure == CZG3_RACE_TIMEOUT_CLEAN
             ? CZG3_SAFE_RETRY
             : CZG3_UNSAFE_OR_UNKNOWN;
}

const char *czg3_writer_phase_name(enum czg3_writer_phase phase) {
  static const char *const names[] = {
      "NOT_ARMED", "ARMED_OUTCOME_UNKNOWN", "RETURNED_CLEANUP_UNPROVEN",
      "WRITER_ENTERED",
      "WRITER_RETURNED_MUTATION_UNCERTAIN", "POSSIBLE_MUTATION",
      "CLEAN_PRE_ENTRY_MISS", "VERIFIED_SUCCESS"};
  return (unsigned)phase < sizeof(names) / sizeof(names[0])
             ? names[phase]
             : "INVALID";
}

enum czg3_retry_safety czg3_writer_retry_policy(
    enum czg3_writer_phase phase) {
  return phase == CZG3_WRITER_NOT_ARMED ||
                 phase == CZG3_WRITER_CLEAN_PRE_ENTRY_MISS
             ? CZG3_SAFE_RETRY
             : CZG3_UNSAFE_OR_UNKNOWN;
}

enum czg3_supervisor_decision czg3_supervisor_decide(
    int child_succeeded, enum czg3_writer_phase phase) {
  if (child_succeeded)
    return CZG3_SUPERVISOR_COMPLETE;
  if (phase == CZG3_WRITER_VERIFIED_SUCCESS)
    return CZG3_SUPERVISOR_REBOOT_REQUIRED;
  return czg3_writer_retry_policy(phase) == CZG3_SAFE_RETRY
             ? CZG3_SUPERVISOR_RETRY
             : CZG3_SUPERVISOR_REBOOT_REQUIRED;
}

void czg3_timing_init(struct czg3_timing *t, int baseline, int minimum,
                      int maximum) {
  t->minimum_usec = minimum;
  t->maximum_usec = maximum;
  t->baseline_usec = baseline < minimum ? minimum :
                     baseline > maximum ? maximum : baseline;
  t->current_usec = t->baseline_usec;
  t->clean_misses = 0;
}

int czg3_timing_clean_miss(struct czg3_timing *t, int direction) {
  if (direction != -1 && direction != 1)
    return t->current_usec;
  int next = t->current_usec + direction * 250;
  if (next < t->minimum_usec) next = t->minimum_usec;
  if (next > t->maximum_usec) next = t->maximum_usec;
  t->current_usec = next;
  t->clean_misses++;
  return next;
}

void czg3_timing_ambiguous(struct czg3_timing *t) {
  t->current_usec = t->baseline_usec;
  t->clean_misses = 0;
}

void czg3_diag_start(const char *profile) {
  started_ns = now_ns(CLOCK_MONOTONIC);
  run_id = (started_ns << 16) ^ (uint64_t)getpid();
  profile_name = profile ? profile : "unset";
  diag_owner_pid = getpid();

  /*
   * Do not allocate the shared race telemetry store here.  CZG3's P0
   * preparation is allocator-sensitive; an extra VMA before KernelSnitch
   * changes the process layout compared with the hardware-validated path.
   * The mapping is created by the first race-enter diagnostic, after P0
   * preparation and immediately before the race child is forked.
   */
  long cpus = sysconf(_SC_NPROCESSORS_ONLN);
  double load = -1.0;
  FILE *load_file = fopen("/proc/loadavg", "re");
  if (load_file) {
    (void)fscanf(load_file, "%lf", &load);
    (void)fclose(load_file);
  }
  fprintf(stdout, "RMG_DIAG_V1|run=%016llx|ts_ns=%llu|elapsed_us=0|stage=RUN_START|attempt=0|failure=SUCCESS|safety=UNSAFE_OR_UNKNOWN|cleanup=0|profile=%s|boottime_ns=%llu|cpus=%ld|load=%.2f\n",
          (unsigned long long)run_id, (unsigned long long)started_ns,
          profile_name, (unsigned long long)now_ns(CLOCK_BOOTTIME), cpus, load);
}

void czg3_diag_event(const char *stage, int attempt,
                     enum czg3_failure failure, int cleanup_complete,
                     const char *state) {
#if defined(CZG3_RACE_TELEMETRY) && CZG3_RACE_TELEMETRY
  if (getpid() == diag_owner_pid && stage &&
      (strcmp(stage, "PHYSICAL_RACE_ENTER") == 0 ||
       strcmp(stage, "FOPS_RACE_ENTER") == 0)) {
    czg3_race_prepare_shared();
  }
  if (getpid() == diag_owner_pid && stage &&
      strcmp(stage, "PHYSICAL_RACE_RESULT") == 0) {
    czg3_race_flush_pending(attempt);
  }
#endif
  uint64_t now = now_ns(CLOCK_MONOTONIC);
  enum czg3_retry_safety safety = czg3_retry_policy(failure, cleanup_complete);
  fprintf(stdout, "RMG_DIAG_V1|run=%016llx|ts_ns=%llu|elapsed_us=%llu|stage=%s|attempt=%d|failure=%s|safety=%s|cleanup=%d|profile=%s|state=%s\n",
          (unsigned long long)run_id, (unsigned long long)now,
          (unsigned long long)((now - started_ns) / 1000), stage, attempt,
          czg3_failure_name(failure), safety == CZG3_SAFE_RETRY ? "SAFE_RETRY" : "UNSAFE_OR_UNKNOWN",
          !!cleanup_complete, profile_name, state ? state : "none");
}

void czg3_diag_checkpoint(const char *stage, int attempt) {
  char record[160];
  int n = snprintf(record, sizeof(record),
                   "RMG_DIAG_V1|run=%016llx|stage=%s|attempt=%d\n",
                   (unsigned long long)run_id, stage, attempt);
  int fd = open("/data/local/tmp/rmg-czg3-last-stage", O_WRONLY | O_CREAT |
                O_TRUNC | O_CLOEXEC | O_DSYNC, 0600);
  if (fd >= 0) {
    (void)write(fd, record, (size_t)n);
    (void)close(fd);
  }
}

#if defined(CZG3_RACE_TELEMETRY) && CZG3_RACE_TELEMETRY
#define RACE_RECORD_CAPACITY 64
#define SNAPSHOT_PHASES 2

struct race_record {
  uint64_t stamp;
  int64_t a0;
  int64_t a1;
  uint16_t event;
};

struct role_trace {
  struct race_record records[RACE_RECORD_CAPACITY];
  uint32_t count;
  uint32_t dropped;
};

struct sched_snapshot {
  uint64_t ts;
  uint64_t affinity0;
  long voluntary_switches;
  long involuntary_switches;
  int tid;
  int cpu;
  int policy;
  int available;
  int error;
  int rusage_available;
  int captured;
  int metadata_only;
};

struct system_snapshot {
  uint64_t ts;
  long online_cpus;
  long voluntary_switches;
  long involuntary_switches;
  int rusage_available;
  int captured;
  int metadata_only;
};

struct race_telemetry_store {
  struct role_trace traces[CZG3_RACE_ROLE_COUNT];
  struct sched_snapshot sched[SNAPSHOT_PHASES][CZG3_RACE_ROLE_COUNT];
  struct system_snapshot system[SNAPSHOT_PHASES];
  uint32_t dump_requested;
  int race_attempt;
  int race_pid;
};

static struct race_telemetry_store local_store;
static struct race_telemetry_store *race_store = &local_store;
static int race_store_initialized;
static int race_store_shared;

/*
 * Counter calibration is process-local rather than inside race_store because
 * czg3_race_reset() intentionally clears the shared store for every race.
 * The calibration is captured in the owner before fork and inherited by the
 * race child.
 */
static uint64_t race_counter_base_ticks;
static uint64_t race_counter_base_ns;
static uint64_t race_counter_freq_hz;

static const char *const race_roles[] = {
    "parent", "owner", "waiter", "consumer"
};

static const char *const race_events[] = {
    "thread_ready", "pselect_prepare_complete", "pselect_enter", "pselect_return",
    "owner_target_locked", "owner_chain_lock_enter", "owner_chain_lock_return",
    "cmp_enter", "cmp_return", "wait_requeue_pi_enter", "wait_requeue_pi_return",
    "waiter_timeout_accepted", "waiter_unlock_enter", "waiter_unlock_return",
    "writer_enter", "writer_return", "consumer_armed", "delay_begin", "delay_end",
    "consumer_action_begin", "readiness_operation_complete", "consumer_action_end"
};

static int phase_index(const char *phase) {
  if (!phase) return -1;
  if (strncmp(phase, "pre", 3) == 0) return 0;
  if (strncmp(phase, "post", 4) == 0) return 1;
  return -1;
}

static uint64_t affinity_word0(const cpu_set_t *affinity) {
  uint64_t value = 0;
  size_t bytes = sizeof(value) < sizeof(*affinity) ? sizeof(value) : sizeof(*affinity);
  memcpy(&value, affinity, bytes);
  return value;
}

#if defined(__aarch64__)
static inline uint64_t race_read_counter(void) {
  uint64_t value;
  __asm__ volatile("mrs %0, cntvct_el0" : "=r"(value));
  return value;
}

static inline uint64_t race_read_frequency(void) {
  uint64_t value;
  __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
  return value;
}
#endif

static uint64_t race_stamp_now(void) {
#if defined(__aarch64__)
  if (race_counter_freq_hz) {
    return race_read_counter();
  }
#endif
  return now_ns(CLOCK_MONOTONIC_RAW);
}

static uint64_t race_stamp_to_ns(uint64_t stamp) {
#if defined(__aarch64__)
  if (race_counter_freq_hz) {
    uint64_t delta = stamp - race_counter_base_ticks;
    uint64_t seconds = delta / race_counter_freq_hz;
    uint64_t remainder = delta % race_counter_freq_hz;
    return race_counter_base_ns +
           seconds * 1000000000ULL +
           (remainder * 1000000000ULL) / race_counter_freq_hz;
  }
#endif
  return stamp;
}

void czg3_race_prepare_shared(void) {
  if (race_store_initialized) return;
  race_store_initialized = 1;

#if defined(__aarch64__)
  race_counter_freq_hz = race_read_frequency();
  if (race_counter_freq_hz) {
    uint64_t before = race_read_counter();
    race_counter_base_ns = now_ns(CLOCK_MONOTONIC_RAW);
    uint64_t after = race_read_counter();
    race_counter_base_ticks = before + (after - before) / 2;
  }
#endif

  void *mapping = mmap(NULL, sizeof(struct race_telemetry_store),
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) {
    race_store = &local_store;
    race_store_shared = 0;
    memset(race_store, 0, sizeof(*race_store));
    return;
  }
  race_store = (struct race_telemetry_store *)mapping;
  race_store_shared = 1;
  memset(race_store, 0, sizeof(*race_store));
}

void czg3_race_reset(int attempt) {
  /*
   * Defensive fallback for non-CZG3 callers that somehow reach a race without
   * the structured race-enter event.  On CZG3 this is already initialized by
   * PHYSICAL_RACE_ENTER/FOPS_RACE_ENTER in the owner before fork.
   */
  if (!race_store_initialized) {
    czg3_race_prepare_shared();
  }
  memset(race_store, 0, sizeof(*race_store));
  race_store->race_attempt = attempt;
  race_store->race_pid = getpid();
}

void czg3_race_record_impl(enum czg3_race_role role,
                           enum czg3_race_event event,
                           int64_t arg0, int64_t arg1) {
  if ((unsigned)role >= CZG3_RACE_ROLE_COUNT) return;
  struct role_trace *trace = &race_store->traces[role];
  uint32_t slot = __atomic_load_n(&trace->count, __ATOMIC_RELAXED);
  if (slot >= RACE_RECORD_CAPACITY) {
    __atomic_fetch_add(&trace->dropped, 1U, __ATOMIC_RELAXED);
    return;
  }
  trace->records[slot] = (struct race_record){
      race_stamp_now(), arg0, arg1, (uint16_t)event};
  __atomic_store_n(&trace->count, slot + 1U, __ATOMIC_RELEASE);
}

void czg3_race_thread_snapshot(const char *phase,
                               enum czg3_race_role role, int tid) {
  int phase_id = phase_index(phase);
  if (phase_id < 0 || (unsigned)role >= CZG3_RACE_ROLE_COUNT || tid <= 0) return;

  struct sched_snapshot snapshot;
  memset(&snapshot, 0, sizeof(snapshot));
  snapshot.tid = tid;
  snapshot.cpu = -1;
  snapshot.policy = -1;

  /*
   * Every pre snapshot is metadata-only.  With telemetry compiled out these
   * calls were no-ops, so scheduler/rusage syscalls before the PI race were a
   * release-only perturbation.  Complete scheduler snapshots remain post-race.
   */
  if (phase_id == 0) {
    snapshot.captured = 1;
    snapshot.metadata_only = 1;
    race_store->sched[phase_id][role] = snapshot;
    return;
  }

  snapshot.ts = now_ns(CLOCK_MONOTONIC_RAW);
  int current_tid = (int)syscall(SYS_gettid);
  if (tid == current_tid) snapshot.cpu = sched_getcpu();

  cpu_set_t affinity;
  CPU_ZERO(&affinity);
  errno = 0;
  if (sched_getaffinity(tid, sizeof(affinity), &affinity) == 0) {
    snapshot.affinity0 = affinity_word0(&affinity);
    snapshot.available = 1;
  } else {
    snapshot.error = errno;
  }

  errno = 0;
  int policy = sched_getscheduler(tid);
  if (policy >= 0) {
    snapshot.policy = policy;
    snapshot.available = 1;
  } else if (!snapshot.error) {
    snapshot.error = errno;
  }

  if (tid == current_tid) {
    struct rusage usage;
    memset(&usage, 0, sizeof(usage));
    if (getrusage(RUSAGE_THREAD, &usage) == 0) {
      snapshot.voluntary_switches = usage.ru_nvcsw;
      snapshot.involuntary_switches = usage.ru_nivcsw;
      snapshot.rusage_available = 1;
      snapshot.available = 1;
    }
  }

  snapshot.captured = 1;
  race_store->sched[phase_id][role] = snapshot;
}

void czg3_race_system_snapshot(const char *phase) {
  int phase_id = phase_index(phase);
  if (phase_id < 0) return;

  struct system_snapshot snapshot;
  memset(&snapshot, 0, sizeof(snapshot));

  if (phase_id == 0) {
    snapshot.captured = 1;
    snapshot.metadata_only = 1;
    race_store->system[phase_id] = snapshot;
    return;
  }

  snapshot.ts = now_ns(CLOCK_MONOTONIC_RAW);
  snapshot.online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
  struct rusage usage;
  memset(&usage, 0, sizeof(usage));
  if (getrusage(RUSAGE_SELF, &usage) == 0) {
    snapshot.voluntary_switches = usage.ru_nvcsw;
    snapshot.involuntary_switches = usage.ru_nivcsw;
    snapshot.rusage_available = 1;
  }
  snapshot.captured = 1;
  race_store->system[phase_id] = snapshot;
}

static void dump_sched_snapshot(int phase_id, enum czg3_race_role role) {
  const struct sched_snapshot *snapshot = &race_store->sched[phase_id][role];
  if (!snapshot->captured) return;
  const char *phase = phase_id == 0 ? "pre" : "post";
  fprintf(stdout,
          "RMG_SCHED_V1|run=%016llx|phase=%s|role=%s|tid=%d|cpu=%d|policy=%d|priority=-1|nice=-1|available=%d|errno=%d|affinity0=%016llx|ts_raw_ns=%llu|mode=%s\n",
          (unsigned long long)run_id, phase, race_roles[role], snapshot->tid,
          snapshot->cpu, snapshot->policy, snapshot->available, snapshot->error,
          (unsigned long long)snapshot->affinity0,
          (unsigned long long)snapshot->ts,
          snapshot->metadata_only ? "hotpath_metadata_only" : "buffered");

  if (snapshot->rusage_available) {
    fprintf(stdout,
            "RMG_SYS_V1|run=%016llx|phase=%s|kind=%s_sched|available=1|line=nr_voluntary_switches : %ld\n",
            (unsigned long long)run_id, phase, race_roles[role],
            snapshot->voluntary_switches);
    fprintf(stdout,
            "RMG_SYS_V1|run=%016llx|phase=%s|kind=%s_sched|available=1|line=nr_involuntary_switches : %ld\n",
            (unsigned long long)run_id, phase, race_roles[role],
            snapshot->involuntary_switches);
  }
}

static void dump_system_snapshot(int phase_id) {
  const struct system_snapshot *snapshot = &race_store->system[phase_id];
  if (!snapshot->captured) return;
  const char *phase = phase_id == 0 ? "pre_fops" : "post_fops";
  fprintf(stdout,
          "RMG_SYS_V1|run=%016llx|phase=%s|kind=online_cpus|available=%d|line=%ld|ts_raw_ns=%llu|mode=%s\n",
          (unsigned long long)run_id, phase,
          snapshot->metadata_only ? 0 : snapshot->online_cpus > 0,
          snapshot->online_cpus, (unsigned long long)snapshot->ts,
          snapshot->metadata_only ? "hotpath_metadata_only" : "buffered");
  if (snapshot->rusage_available) {
    fprintf(stdout,
            "RMG_SYS_V1|run=%016llx|phase=%s|kind=process_rusage|available=1|line=voluntary_ctxt_switches: %ld\n",
            (unsigned long long)run_id, phase, snapshot->voluntary_switches);
    fprintf(stdout,
            "RMG_SYS_V1|run=%016llx|phase=%s|kind=process_rusage|available=1|line=nonvoluntary_ctxt_switches: %ld\n",
            (unsigned long long)run_id, phase, snapshot->involuntary_switches);
  }
}

static void czg3_race_dump_now(int fallback_attempt) {
  int attempt = race_store->race_attempt > 0
                    ? race_store->race_attempt
                    : fallback_attempt;
  int race_pid = race_store->race_pid > 0
                     ? race_store->race_pid
                     : (int)diag_owner_pid;
  uint32_t dropped = 0;
  for (int role = 0; role < CZG3_RACE_ROLE_COUNT; role++) {
    struct role_trace *trace = &race_store->traces[role];
    uint32_t count = __atomic_load_n(&trace->count, __ATOMIC_ACQUIRE);
    dropped += __atomic_load_n(&trace->dropped, __ATOMIC_RELAXED);
    for (uint32_t i = 0; i < count; i++) {
      const struct race_record *record = &trace->records[i];
      const char *event =
          record->event < sizeof(race_events) / sizeof(race_events[0])
              ? race_events[record->event]
              : "invalid";
      fprintf(stdout,
              "RMG_RACE_V1|run=%016llx|attempt=%d|race=%d|role=%s|event=%s|ts_raw_ns=%llu|arg0=%lld|arg1=%lld\n",
              (unsigned long long)run_id, attempt, race_pid, race_roles[role],
              event, (unsigned long long)race_stamp_to_ns(record->stamp),
              (long long)record->a0, (long long)record->a1);
    }
  }

  fprintf(stdout,
          "RMG_RACE_V1|run=%016llx|attempt=%d|race=%d|role=parent|event=trace_status|ts_raw_ns=%llu|trace_complete=%d|dropped_events=%u|mode=buffered_no_hotpath_io|clock=%s\n",
          (unsigned long long)run_id, attempt, race_pid,
          (unsigned long long)now_ns(CLOCK_MONOTONIC_RAW),
          dropped == 0, dropped,
          race_counter_freq_hz ? "cntvct_el0" : "clock_monotonic_raw");

  for (int phase = 0; phase < SNAPSHOT_PHASES; phase++) {
    for (int role = 0; role < CZG3_RACE_ROLE_COUNT; role++) {
      dump_sched_snapshot(phase, (enum czg3_race_role)role);
    }
    dump_system_snapshot(phase);
  }
  fprintf(stdout,
          "RMG_SYS_V1|run=%016llx|phase=post_fops|kind=telemetry_mode|available=1|line=buffered_shared_deferred_dump_arch_counter_hotpath_metadata_only\n",
          (unsigned long long)run_id);
  fflush(stdout);
}

void czg3_race_dump(void) {
  if (race_store_shared) {
    __atomic_store_n(&race_store->dump_requested, 1U, __ATOMIC_RELEASE);
    return;
  }
  if (getpid() == diag_owner_pid) {
    czg3_race_dump_now(race_store->race_attempt);
  }
}

void czg3_race_flush_pending(int fallback_attempt) {
  if (!race_store_initialized) {
    return;
  }
  if (!race_store_shared) {
    fprintf(stdout,
            "RMG_RACE_V1|run=%016llx|attempt=%d|race=%d|role=parent|event=trace_status|ts_raw_ns=%llu|trace_complete=0|dropped_events=0|mode=shared_buffer_unavailable\n",
            (unsigned long long)run_id, fallback_attempt, (int)diag_owner_pid,
            (unsigned long long)now_ns(CLOCK_MONOTONIC_RAW));
    fprintf(stdout,
            "RMG_SYS_V1|run=%016llx|phase=post_fops|kind=telemetry_mode|available=0|line=shared_buffer_unavailable\n",
            (unsigned long long)run_id);
    return;
  }

  uint32_t requested = __atomic_exchange_n(&race_store->dump_requested, 0U,
                                            __ATOMIC_ACQ_REL);
  if (!requested) return;
  czg3_race_dump_now(fallback_attempt);
}
#endif
