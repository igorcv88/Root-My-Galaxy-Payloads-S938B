#ifndef BOOT_CONTROL_H
#define BOOT_CONTROL_H

#define RMG_BOOT_MIN_UPTIME_DEFAULT_SEC 120
#define RMG_BOOT_MIN_UPTIME_MAX_SEC 600

int rmg_parse_boot_min_uptime_sec(const char *value);

#endif
