#include <asm/unistd.h>

    .text
    .align 2
    .global __wrap_syscall
    .type __wrap_syscall, %function
__wrap_syscall:
    cmp x0, #__NR_pselect6
    b.eq czg3_auto_pselect_dispatch
    b __real_syscall
    .size __wrap_syscall, .-__wrap_syscall
