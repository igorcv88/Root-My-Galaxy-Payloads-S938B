# CZG3 race telemetry collection

This diagnostic channel observes the `SM-S938B` CZG3 FOPS race without changing its delay, timeout, affinity, scheduler policy, allocator preparation, ordering, retries, or fail-closed decisions. `make ... diagnostic` enables `CZG3_RACE_TELEMETRY`; normal `release` builds do not contain the race trace or snapshot collectors.

The hot path writes timestamped numeric records to fixed 64-record per-role arrays. It performs no allocation, formatting, file I/O, or locking. Records use `CLOCK_MONOTONIC_RAW` and are formatted as `RMG_RACE_V1` only after the race completes. `RMG_SCHED_V1` and `RMG_SYS_V1` snapshots are collected before arming and after completion. Missing optional files are reported with `available=0` and never affect exploit safety.

## Controlled protocol

1. Install the diagnostic APK and cache the diagnostic payload through a successful verified install.
2. Confirm that the cached payload size and SHA-256 match `support/targets-v3-diagnostic.json`.
3. Do not change race parameters between samples.
4. Use a real/full kernel reboot for every sample; do not use the KernelSU soft reboot.
5. Do not manually root between Auto Root samples.
6. After boot, do not interact with the phone until Auto Root terminates.
7. Export every diagnostic report, successful or failed.
8. Initially collect about 10 successes and 5 failures, or 20-25 real boots if failures are rarer.
9. Stop and analyze the complete batch before tuning anything.

Compare consumer-to-readiness and readiness-to-pselect-return timing, writer timing, CPU migration and involuntary-switch deltas, CPU frequency/state, PSI, thermal state, softirq/interrupt deltas, and allocator/reclaim counters. These are measurements and correlations, not causal classifications. After a repeatable separator is found, change one variable at a time in later A/B builds.
