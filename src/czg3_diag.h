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
void czg3_timing_init(struct czg3_timing *timing, int baseline,
                      int minimum, int maximum);
int czg3_timing_clean_miss(struct czg3_timing *timing, int direction);
void czg3_timing_ambiguous(struct czg3_timing *timing);
void czg3_diag_start(const char *profile);
void czg3_diag_event(const char *stage, int attempt,
                     enum czg3_failure failure, int cleanup_complete,
                     const char *state);
void czg3_diag_checkpoint(const char *stage, int attempt);

#endif
