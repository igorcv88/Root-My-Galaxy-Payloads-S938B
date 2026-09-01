#include "czg3_diag.h"
#include "boot_control.h"
#include <assert.h>
#include <string.h>

int main(void) {
  char prep_record[320];
  int prep_length = czg3_prep_format_record(
      prep_record, sizeof(prep_record), 0x1234, 2, "p0",
      "kernelsnitch_setup", 99, 42, "ok", 7, 8);
  assert(prep_length > 0 && prep_length < (int)sizeof(prep_record));
  assert(!strcmp(
      prep_record,
      "RMG_PREP_V1|run=0000000000001234|attempt=2|scope=p0|event=kernelsnitch_setup|ts_raw=99|duration_us=42|result=ok|arg0=7|arg1=8"));
  assert(rmg_parse_boot_min_uptime_sec(NULL) == 120);
  assert(rmg_parse_boot_min_uptime_sec("") == 120);
  assert(rmg_parse_boot_min_uptime_sec("0") == 0);
  assert(rmg_parse_boot_min_uptime_sec("30") == 30);
  assert(rmg_parse_boot_min_uptime_sec("60") == 60);
  assert(rmg_parse_boot_min_uptime_sec("120") == 120);
  assert(rmg_parse_boot_min_uptime_sec("600") == 600);
  assert(rmg_parse_boot_min_uptime_sec("-1") == 120);
  assert(rmg_parse_boot_min_uptime_sec("invalid") == 120);
  assert(rmg_parse_boot_min_uptime_sec("601") == 120);
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
