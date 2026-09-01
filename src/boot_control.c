#include "boot_control.h"

#include <errno.h>
#include <stdlib.h>

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
