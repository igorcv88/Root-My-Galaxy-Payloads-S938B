#include "boot_control.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
 * Fast immutable process-mode flag for the CZG3 syscall wrapper.
 *
 * The Auto Root SIGRETURN experiment must not route Manual or P0 pselect6
 * calls through a C dispatcher. Device telemetry showed that doing so was
 * enough to reproduce the ~40 ms blocked sched_setattr phenotype even in
 * manual_online/top-app. Constructors complete before any exploit attempt, and
 * RMG_INVOCATION_MODE does not change during the payload lifetime, so cache it
 * once and let the assembly wrapper tail-call libc directly for non-Auto work.
 */
int czg3_auto_root_invocation_fast;

__attribute__((constructor)) static void rmg_init_auto_root_invocation_fast(void) {
  const char *mode = getenv("RMG_INVOCATION_MODE");
  czg3_auto_root_invocation_fast =
      mode && strcmp(mode, "auto_root") == 0;
}

int rmg_parse_boot_min_uptime_sec(const char *value) {
  if (!value || !*value) {
    return RMG_BOOT_MIN_UPTIME_DEFAULT_SEC;
  }

  char *end = NULL;
  errno = 0;
  long parsed = strtol(value, &end, 10);
  if (errno || end == value || *end || parsed < 0 ||
      parsed > RMG_BOOT_MIN_UPTIME_MAX_SEC) {
    return RMG_BOOT_MIN_UPTIME_DEFAULT_SEC;
  }
  return (int)parsed;
}
