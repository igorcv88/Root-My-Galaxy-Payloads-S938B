# CZG3 no-global-syscall-wrap experiment

Purpose: isolate the intermittent Manual FOPS `sched_setattr_tid()` stall observed after Auto SIGRETURN support was introduced.

Device evidence with payload SHA256 `6372b85e2c7bd6f5697bb841ce7bdf161dd084618cf89bdfe4c5cbd5d7908ad9`:

- healthy Manual run: action-to-readiness 91 us, FOPS `window=1`, root succeeded;
- second healthy FOPS race: action-to-readiness 55 us, FOPS `window=1`, trigger succeeded, later failed independently at the physrw cache gate;
- anomalous Manual run: `RMG_CZG3_GATE_V1|proxy_us=0|live_us=140|real_sched_us=39937`, proving the ~40 ms delay was inside the real `sched_setattr_tid()` call rather than the Auto proxy or `/proc` live check.

Experiment:

- remove `src/czg3_syscall_wrap.S` from the CZG3 release source list;
- remove `-Wl,--wrap=syscall` entirely;
- preserve `--wrap=usleep` and `--wrap=sched_setattr_tid` unchanged;
- keep Manual/P0 on direct libc `syscall()`;
- fail closed Auto Root in a constructor before any exploit race because dedicated Auto SIGRETURN interception is intentionally disconnected in this diagnostic build;
- retain the existing `RMG_CZG3_GATE_V1` split, External Observer v2, P0 oracle, cache gate, physrw validation, mutation uncertainty and reboot policy.

Acceptance test: fresh boot, Manual Standalone only. Do not alter the 60000 us FOPS coarse delay or 120 s minimum uptime. A healthy FOPS sample should remain sub-ms. If a slow sample occurs, use `RMG_CZG3_GATE_V1 real_sched_us` as the decisive metric.

This branch is diagnostic and must not be merged as the final Auto Root architecture. After Manual validation, reconnect Auto SIGRETURN at a dedicated Auto-FOPS call site without restoring linker-wide syscall interposition.
