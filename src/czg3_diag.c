#define _GNU_SOURCE
#include "czg3_diag.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
