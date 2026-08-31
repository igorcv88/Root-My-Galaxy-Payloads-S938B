#ifndef CZG3_DIAG_H
#define CZG3_DIAG_H

#include <stdint.h>

enum czg3_failure {
  CZG3_PRECONDITION_FAILED,
  CZG3_P0_DISCOVERY_FAILED,
  CZG3_P0_STATE_INVALID,
  CZG3_RACE_NOT_WON,
  CZG3_RACE_TIMEOUT_CLEAN,
  CZG3_RACE_STATE_UNCERTAIN,
  CZG3_CLEANUP_FAILED,
  CZG3_PRIMITIVE_VALIDATION_FAILED,
  CZG3_PRIVILEGE_BOOTSTRAP_FAILED,
  CZG3_SUCCESS
};

enum czg3_retry_safety { CZG3_SAFE_RETRY, CZG3_UNSAFE_OR_UNKNOWN };
enum czg3_supervisor_decision {
  CZG3_SUPERVISOR_COMPLETE,
  CZG3_SUPERVISOR_RETRY,
  CZG3_SUPERVISOR_REBOOT_REQUIRED
};

enum czg3_writer_phase {
  CZG3_WRITER_NOT_ARMED,
  CZG3_WRITER_ARMED,
  CZG3_WRITER_RETURNED_CLEANUP_UNPROVEN,
  CZG3_WRITER_ENTERED,
  CZG3_WRITER_RETURNED_UNCERTAIN,
  CZG3_WRITER_POSSIBLE_MUTATION,
  CZG3_WRITER_CLEAN_PRE_ENTRY_MISS,
  CZG3_WRITER_VERIFIED_SUCCESS
};

struct czg3_timing {
  int baseline_usec;
  int current_usec;
  int minimum_usec;
  int maximum_usec;
  int clean_misses;
};

const char *czg3_failure_name(enum czg3_failure failure);
enum czg3_retry_safety czg3_retry_policy(enum czg3_failure failure,
                                          int cleanup_complete);
const char *czg3_writer_phase_name(enum czg3_writer_phase phase);
enum czg3_retry_safety czg3_writer_retry_policy(
    enum czg3_writer_phase phase);
enum czg3_supervisor_decision czg3_supervisor_decide(
    int child_succeeded, enum czg3_writer_phase phase);
void czg3_timing_init(struct czg3_timing *timing, int baseline,
                      int minimum, int maximum);
int czg3_timing_clean_miss(struct czg3_timing *timing, int direction);
void czg3_timing_ambiguous(struct czg3_timing *timing);
void czg3_diag_start(const char *profile);
void czg3_diag_event(const char *stage, int attempt,
                     enum czg3_failure failure, int cleanup_complete,
                     const char *state);
void czg3_diag_checkpoint(const char *stage, int attempt);

#if defined(CZG3_RACE_TELEMETRY) && CZG3_RACE_TELEMETRY
enum czg3_race_role {
  CZG3_RACE_PARENT,
  CZG3_RACE_OWNER,
  CZG3_RACE_WAITER,
  CZG3_RACE_CONSUMER,
  CZG3_RACE_ROLE_COUNT
};

enum czg3_race_event {
  CZG3_RACE_THREAD_READY,
  CZG3_RACE_PSELECT_PREPARE_COMPLETE,
  CZG3_RACE_PSELECT_ENTER,
  CZG3_RACE_PSELECT_RETURN,
  CZG3_RACE_OWNER_TARGET_LOCKED,
  CZG3_RACE_OWNER_CHAIN_LOCK_ENTER,
  CZG3_RACE_OWNER_CHAIN_LOCK_RETURN,
  CZG3_RACE_CMP_ENTER,
  CZG3_RACE_CMP_RETURN,
  CZG3_RACE_WAIT_REQUEUE_ENTER,
  CZG3_RACE_WAIT_REQUEUE_RETURN,
  CZG3_RACE_WAITER_TIMEOUT_ACCEPTED,
  CZG3_RACE_WAITER_UNLOCK_ENTER,
  CZG3_RACE_WAITER_UNLOCK_RETURN,
  CZG3_RACE_WRITER_ENTER,
  CZG3_RACE_WRITER_RETURN,
  CZG3_RACE_CONSUMER_ARMED,
  CZG3_RACE_DELAY_BEGIN,
  CZG3_RACE_DELAY_END,
  CZG3_RACE_CONSUMER_ACTION_BEGIN,
  CZG3_RACE_READINESS_OPERATION_COMPLETE,
  CZG3_RACE_CONSUMER_ACTION_END
};

void czg3_race_reset(int attempt);
void czg3_race_record(enum czg3_race_role role, enum czg3_race_event event,
                      int64_t arg0, int64_t arg1);
void czg3_race_dump(void);
void czg3_race_system_snapshot(const char *phase);
void czg3_race_thread_snapshot(const char *phase,
                               enum czg3_race_role role, int tid);
#else
#define czg3_race_reset(attempt) ((void)0)
#define czg3_race_record(role, event, arg0, arg1) ((void)0)
#define czg3_race_dump() ((void)0)
#define czg3_race_system_snapshot(phase) ((void)0)
#define czg3_race_thread_snapshot(phase, role, tid) ((void)0)
#endif

#endif
