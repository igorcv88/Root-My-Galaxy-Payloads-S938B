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
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#endif

static uint64_t run_id;
static uint64_t started_ns;
static const char *profile_name = "unset";

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
    return t->current_usec; /* no measured direction: do not invent one */
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
#define SNAPSHOT_LINE_CAPACITY 512
struct race_record { uint64_t ts; int64_t a0; int64_t a1; uint16_t event; };
struct role_trace { struct race_record records[RACE_RECORD_CAPACITY]; uint32_t count; uint32_t dropped; };
static struct role_trace race_traces[CZG3_RACE_ROLE_COUNT];
static int race_attempt;
static const char *const race_roles[] = {"parent", "owner", "waiter", "consumer"};
static const char *const race_events[] = {
  "thread_ready", "pselect_prepare_complete", "pselect_enter", "pselect_return",
  "owner_target_locked", "owner_chain_lock_enter", "owner_chain_lock_return",
  "cmp_enter", "cmp_return", "wait_requeue_pi_enter", "wait_requeue_pi_return",
  "waiter_timeout_accepted", "waiter_unlock_enter", "waiter_unlock_return",
  "writer_enter", "writer_return", "consumer_armed", "delay_begin", "delay_end",
  "consumer_action_begin", "readiness_operation_complete", "consumer_action_end"
};

void czg3_race_reset(int attempt) { memset(race_traces, 0, sizeof(race_traces)); race_attempt = attempt; }

void czg3_race_record(enum czg3_race_role role, enum czg3_race_event event,
                      int64_t arg0, int64_t arg1) {
  if ((unsigned)role >= CZG3_RACE_ROLE_COUNT) return;
  struct role_trace *trace = &race_traces[role];
  uint32_t slot = trace->count;
  if (slot >= RACE_RECORD_CAPACITY) { trace->dropped++; return; }
  trace->records[slot] = (struct race_record){now_ns(CLOCK_MONOTONIC_RAW), arg0, arg1, (uint16_t)event};
  trace->count = slot + 1;
}

void czg3_race_dump(void) {
  uint32_t dropped = 0;
  for (int role = 0; role < CZG3_RACE_ROLE_COUNT; role++) {
    struct role_trace *trace = &race_traces[role]; dropped += trace->dropped;
    for (uint32_t i = 0; i < trace->count; i++) {
      const struct race_record *r = &trace->records[i];
      const char *event = r->event < sizeof(race_events) / sizeof(race_events[0]) ? race_events[r->event] : "invalid";
      fprintf(stdout, "RMG_RACE_V1|run=%016llx|attempt=%d|race=%d|role=%s|event=%s|ts_raw_ns=%llu|arg0=%lld|arg1=%lld\n",
              (unsigned long long)run_id, race_attempt, getpid(), race_roles[role], event,
              (unsigned long long)r->ts, (long long)r->a0, (long long)r->a1);
    }
  }
  fprintf(stdout, "RMG_RACE_V1|run=%016llx|attempt=%d|race=%d|role=parent|event=trace_status|ts_raw_ns=%llu|trace_complete=%d|dropped_events=%u\n",
          (unsigned long long)run_id, race_attempt, getpid(),
          (unsigned long long)now_ns(CLOCK_MONOTONIC_RAW), dropped == 0, dropped);
}

static void dump_file(const char *phase, const char *kind, const char *path, int max_lines) {
  FILE *file = fopen(path, "re");
  if (!file) {
    fprintf(stdout, "RMG_SYS_V1|run=%016llx|phase=%s|kind=%s|available=0|errno=%d\n",
            (unsigned long long)run_id, phase, kind, errno); return;
  }
  char line[SNAPSHOT_LINE_CAPACITY]; int lines = 0;
  while (lines++ < max_lines && fgets(line, sizeof(line), file)) {
    line[strcspn(line, "\r\n")] = 0;
    fprintf(stdout, "RMG_SYS_V1|run=%016llx|phase=%s|kind=%s|available=1|line=%s\n",
            (unsigned long long)run_id, phase, kind, line);
  }
  fclose(file);
}

void czg3_race_system_snapshot(const char *phase) {
  static const struct { const char *kind; const char *path; int lines; } files[] = {
    {"loadavg", "/proc/loadavg", 1}, {"stat", "/proc/stat", 64},
    {"psi_cpu", "/proc/pressure/cpu", 2}, {"psi_memory", "/proc/pressure/memory", 2},
    {"psi_io", "/proc/pressure/io", 2}, {"meminfo", "/proc/meminfo", 64},
    {"vmstat", "/proc/vmstat", 128}, {"softirqs", "/proc/softirqs", 64},
    {"interrupts", "/proc/interrupts", 96}
  };
  for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) dump_file(phase, files[i].kind, files[i].path, files[i].lines);
  for (int cpu = 0; cpu < 16; cpu++) {
    char path[160], kind[48]; const char *names[] = {"scaling_cur_freq", "scaling_min_freq", "scaling_max_freq", "scaling_governor"};
    for (size_t n = 0; n < sizeof(names) / sizeof(names[0]); n++) {
      snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/%s", cpu, names[n]);
      snprintf(kind, sizeof(kind), "cpu%d_%s", cpu, names[n]); dump_file(phase, kind, path, 1);
    }
  }
  for (int zone = 0; zone < 64; zone++) {
    char path[160], kind[48]; snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/type", zone);
    if (access(path, R_OK) != 0) continue;
    snprintf(kind, sizeof(kind), "thermal%d_type", zone); dump_file(phase, kind, path, 1);
    snprintf(path, sizeof(path), "/sys/class/thermal/thermal_zone%d/temp", zone);
    snprintf(kind, sizeof(kind), "thermal%d_temp", zone); dump_file(phase, kind, path, 1);
  }
}

void czg3_race_thread_snapshot(const char *phase, enum czg3_race_role role, int tid) {
  char path[128], kind[64]; cpu_set_t affinity; CPU_ZERO(&affinity);
  int cpu = sched_getcpu(), policy = sched_getscheduler(tid), affinity_ok = sched_getaffinity(tid, sizeof(affinity), &affinity) == 0;
  struct sched_param param = {0}; int param_ok = sched_getparam(tid, &param) == 0;
  errno = 0; int nice_value = getpriority(PRIO_PROCESS, (id_t)tid); int nice_errno = errno;
  fprintf(stdout, "RMG_SCHED_V1|run=%016llx|phase=%s|role=%s|tid=%d|cpu=%d|policy=%d|priority=%d|nice=%d|available=%d|errno=%d|affinity0=%016llx\n",
          (unsigned long long)run_id, phase, race_roles[role], tid, cpu, policy,
          param.sched_priority, nice_value, policy >= 0 && param_ok && !nice_errno && affinity_ok,
          policy < 0 || !param_ok || nice_errno || !affinity_ok ? errno : 0,
          (unsigned long long)(affinity_ok ? *(const uint64_t *)&affinity : 0));
  const char *files[] = {"sched", "stat", "status"};
  for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
    snprintf(path, sizeof(path), "/proc/self/task/%d/%s", tid, files[i]);
    snprintf(kind, sizeof(kind), "%s_%s", race_roles[role], files[i]); dump_file(phase, kind, path, 128);
  }
}
#endif
