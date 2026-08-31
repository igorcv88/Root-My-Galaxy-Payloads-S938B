#include "czg3_diag.h"
#include <assert.h>
#include <string.h>

int main(void) {
  assert(!strcmp(czg3_failure_name(CZG3_RACE_NOT_WON), "RACE_NOT_WON"));
  assert(czg3_retry_policy(CZG3_RACE_NOT_WON, 1) == CZG3_SAFE_RETRY);
  assert(czg3_retry_policy(CZG3_RACE_NOT_WON, 0) == CZG3_UNSAFE_OR_UNKNOWN);
  assert(czg3_retry_policy(CZG3_RACE_STATE_UNCERTAIN, 1) == CZG3_UNSAFE_OR_UNKNOWN);
  assert(czg3_writer_retry_policy(CZG3_WRITER_NOT_ARMED) == CZG3_SAFE_RETRY);
  assert(czg3_writer_retry_policy(CZG3_WRITER_ARMED) ==
         CZG3_UNSAFE_OR_UNKNOWN);
  assert(czg3_writer_retry_policy(CZG3_WRITER_RETURNED_CLEANUP_UNPROVEN) ==
         CZG3_UNSAFE_OR_UNKNOWN);
  assert(czg3_writer_retry_policy(CZG3_WRITER_CLEAN_PRE_ENTRY_MISS) ==
         CZG3_SAFE_RETRY);
  assert(czg3_writer_retry_policy(CZG3_WRITER_RETURNED_UNCERTAIN) ==
         CZG3_UNSAFE_OR_UNKNOWN);
  assert(czg3_writer_retry_policy(CZG3_WRITER_POSSIBLE_MUTATION) ==
         CZG3_UNSAFE_OR_UNKNOWN);
  assert(!strcmp(czg3_writer_phase_name(CZG3_WRITER_ENTERED),
                 "WRITER_ENTERED"));
  assert(czg3_supervisor_decide(0, CZG3_WRITER_NOT_ARMED) ==
         CZG3_SUPERVISOR_RETRY);
  assert(czg3_supervisor_decide(0, CZG3_WRITER_ARMED) ==
         CZG3_SUPERVISOR_REBOOT_REQUIRED);
  assert(czg3_supervisor_decide(0, CZG3_WRITER_CLEAN_PRE_ENTRY_MISS) ==
         CZG3_SUPERVISOR_RETRY);
  assert(czg3_supervisor_decide(0, CZG3_WRITER_POSSIBLE_MUTATION) ==
         CZG3_SUPERVISOR_REBOOT_REQUIRED);
  assert(czg3_supervisor_decide(1, CZG3_WRITER_VERIFIED_SUCCESS) ==
         CZG3_SUPERVISOR_COMPLETE);
  assert(czg3_supervisor_decide(0, CZG3_WRITER_VERIFIED_SUCCESS) ==
         CZG3_SUPERVISOR_REBOOT_REQUIRED);
  struct czg3_timing t;
  czg3_timing_init(&t, 50000, 49000, 51000);
  for (int i = 0; i < 20; i++) czg3_timing_clean_miss(&t, 1);
  assert(t.current_usec == 51000);
  czg3_timing_ambiguous(&t);
  assert(t.current_usec == 50000 && t.clean_misses == 0);
  assert(czg3_timing_clean_miss(&t, 0) == 50000);
  return 0;
}
