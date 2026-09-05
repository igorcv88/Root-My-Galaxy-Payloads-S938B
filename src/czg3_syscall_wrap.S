#include <asm/unistd.h>

    .text
    .align 2
    .global __wrap_syscall
    .type __wrap_syscall, %function
__wrap_syscall:
    cmp x0, #__NR_pselect6
    b.ne .Lreal_syscall

    /*
     * Manual/P0 must remain on the proven direct syscall path. The original
     * Auto Root SIGRETURN wrapper sent every pselect6 through a C dispatcher;
     * device telemetry then reproduced the ~40 ms blocked sched_setattr
     * phenotype in manual_online too. Keep the direct libc syscall entry path
     * unless this process is Auto Root and the FOPS route is actually active.
     */
    adrp x9, :got:czg3_auto_root_invocation_fast
    ldr x9, [x9, :got_lo12:czg3_auto_root_invocation_fast]
    ldr w9, [x9]
    cbz w9, .Lreal_syscall

    /*
     * Auto Root still uses the established pselect writer for P0. Enter the C
     * dispatcher only after the FOPS route has installed fake_fops as the
     * oracle parent. The dispatcher re-checks the complete parent/target
     * predicate and SIGRETURN preflight fail-closed before writer mutation.
     */
    adrp x9, :got:slide_oracle_parent
    ldr x9, [x9, :got_lo12:slide_oracle_parent]
    ldr x10, [x9]
    adrp x9, :got:fake_fops
    ldr x9, [x9, :got_lo12:fake_fops]
    ldr x11, [x9]
    cbz x11, .Lreal_syscall
    cmp x10, x11
    b.ne .Lreal_syscall

    b czg3_auto_pselect_dispatch

.Lreal_syscall:
    b __real_syscall
    .size __wrap_syscall, .-__wrap_syscall
