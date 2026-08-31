#include "czg3_diag.h"
#include <assert.h>
#include <string.h>

int main(void) {
  assert(!strcmp(czg3_failure_name(CZG3_RACE_NOT_WON), "RACE_NOT_WON"));
  assert(czg3_retry_policy(CZG3_RACE_NOT_WON, 1) == CZG3_SAFE_RETRY);
  assert(czg3_retry_policy(CZG3_RACE_NOT_WON, 0) == CZG3_UNSAFE_OR_UNKNOWN);
  assert(czg3_retry_policy(CZG3_RACE_STATE_UNCERTAIN, 1) == CZG3_UNSAFE_OR_UNKNOWN);
  struct czg3_timing t;
  czg3_timing_init(&t, 50000, 49000, 51000);
  for (int i = 0; i < 20; i++) czg3_timing_clean_miss(&t, 1);
  assert(t.current_usec == 51000);
  czg3_timing_ambiguous(&t);
  assert(t.current_usec == 50000 && t.clean_misses == 0);
  assert(czg3_timing_clean_miss(&t, 0) == 50000);
  return 0;
}
