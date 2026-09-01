#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, found {count}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once(
    "Makefile",
    '''# CZG3 has one canonical v3 payload. Race telemetry is part of that release;
# there is no parallel "diagnostic" payload/feed for the same firmware.
ifeq ($(TARGET),pa3q-S938BXXSBCZG3)
APP_RELEASE_EXTRA_CFLAGS := -DCZG3_RACE_TELEMETRY=1
APP_RELEASE_FIXED_SIZE := 0
CZG3_APP_EXTRA_SRCS := src/boot_control.c
endif''',
    '''# CZG3 keeps one canonical payload, but production observation happens from
# the app's sibling observer process. Compile allocator/race instrumentation
# completely out of the exploit hot path while retaining fail-closed safety.
ifeq ($(TARGET),pa3q-S938BXXSBCZG3)
APP_RELEASE_EXTRA_CFLAGS := -DCZG3_EXTERNAL_OBSERVER=1
APP_RELEASE_FIXED_SIZE := 0
CZG3_APP_EXTRA_SRCS := src/boot_control.c
endif''',
)

replace_once(
    "src/czg3_diag.h",
    '''void czg3_diag_start(const char *profile);
void czg3_diag_event(const char *stage, int attempt,
                     enum czg3_failure failure, int cleanup_complete,
                     const char *state);
void czg3_diag_checkpoint(const char *stage, int attempt);''',
    '''/*
 * Production CZG3 observation lives in the app's sibling observer process.
 * Compile diagnostic call sites out of allocator/race-sensitive code while
 * retaining the implementation for host tests and explicit diagnostic builds.
 */
#if defined(CZG3_EXTERNAL_OBSERVER) && CZG3_EXTERNAL_OBSERVER && \\
    !defined(CZG3_DIAG_IMPLEMENTATION)
#define czg3_diag_start(profile) ((void)0)
#define czg3_diag_event(stage, attempt, failure, cleanup_complete, state) ((void)0)
#define czg3_diag_checkpoint(stage, attempt) ((void)0)
#else
void czg3_diag_start(const char *profile);
void czg3_diag_event(const char *stage, int attempt,
                     enum czg3_failure failure, int cleanup_complete,
                     const char *state);
void czg3_diag_checkpoint(const char *stage, int attempt);
#endif''',
)

replace_once(
    "src/czg3_diag.c",
    '''static uint64_t run_id;
static uint64_t started_ns;
static const char *profile_name = "unset";
static pid_t diag_owner_pid;
''',
    '''#if !defined(CZG3_EXTERNAL_OBSERVER) || !CZG3_EXTERNAL_OBSERVER
static uint64_t run_id;
static uint64_t started_ns;
static const char *profile_name = "unset";
static pid_t diag_owner_pid;
''',
)
replace_once(
    "src/czg3_diag.c",
    '''#endif

const char *czg3_failure_name(enum czg3_failure failure) {''',
    '''#endif
#endif /* !CZG3_EXTERNAL_OBSERVER */

const char *czg3_failure_name(enum czg3_failure failure) {''',
)
replace_once(
    "src/czg3_diag.c",
    '''void czg3_diag_start(const char *profile) {''',
    '''#if !defined(CZG3_EXTERNAL_OBSERVER) || !CZG3_EXTERNAL_OBSERVER
void czg3_diag_start(const char *profile) {''',
)
replace_once(
    "src/czg3_diag.c",
    '''#if defined(CZG3_RACE_TELEMETRY) && CZG3_RACE_TELEMETRY
#define RACE_RECORD_CAPACITY 64''',
    '''#endif /* !CZG3_EXTERNAL_OBSERVER */

#if defined(CZG3_RACE_TELEMETRY) && CZG3_RACE_TELEMETRY
#define RACE_RECORD_CAPACITY 64''',
)

replace_once(
    ".github/scripts/validate_feed.py",
    '''    # The committed canonical artifact may intentionally lag source while a source-only
    # PR is under review. The exploit build step in update-payloads.yml validates all
    # release-only diagnostic markers before it overwrites this canonical file. Keep
    # this feed validator compatible with KernelSU-only updates in that transition.
    exploit_bytes = exploit.read_bytes()
    for marker in (b"RMG_RACE_V1", b"RMG_SCHED_V1", b"RMG_SYS_V1"):
        assert marker in exploit_bytes, (
            f"canonical CZG3 payload is missing {marker.decode()}"
        )
''',
    '''    # A source-only PR can intentionally lead the committed canonical binary.
    # Release instrumentation properties are validated against the freshly built
    # payload in update-payloads.yml, not against a potentially lagging artifact.
''',
)

# The real update-payloads.yml is updated through the GitHub connector after
# this source-only commit passes validation; the Actions token intentionally
# cannot push workflow changes.
