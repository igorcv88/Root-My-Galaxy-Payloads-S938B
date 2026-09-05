#include "common.h"

/*
 * Diagnostic bridge for the experiment that removes -Wl,--wrap=syscall.
 *
 * czg3_auto_sigreturn.c still contains the dormant dispatcher while this
 * Manual/P0 experiment is running. Resolve its __real_syscall reference to the
 * ordinary libc syscall entry without reintroducing any linker interposition.
 * The dispatcher is not reachable from syscall() in this build.
 */
long __real_syscall(long number, long arg1, long arg2, long arg3,
                    long arg4, long arg5, long arg6) {
  return syscall(number, arg1, arg2, arg3, arg4, arg5, arg6);
}

/*
 * Auto Root depends on the dedicated SIGRETURN pselect interception that this
 * experiment intentionally disconnects. Refuse Auto before the exploit race
 * starts instead of silently falling back to the known-bad pselect writer.
 */
__attribute__((constructor)) static void czg3_no_syscall_wrap_auto_guard(void) {
  const char *mode = getenv("RMG_INVOCATION_MODE");
  if (mode && strcmp(mode, "auto_root") == 0) {
    pr_error("CZG3 diagnostic build: Auto Root refused because global syscall "
             "interception is disabled; use Manual for this experiment\n");
    _exit(125);
  }
}
