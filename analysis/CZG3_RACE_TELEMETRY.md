# CZG3 race telemetry

The canonical `SM-S938B` CZG3 release includes race telemetry without changing the exploit's delay, timeout, affinity, scheduler policy, allocator preparation, ordering, retries, or fail-closed decisions. There is no separate diagnostic payload/feed for this firmware: `make TARGET=pa3q-S938BXXSBCZG3 release` enables `CZG3_RACE_TELEMETRY`, and `support/targets-v3.json` points to that release.

The hot path writes timestamped numeric records to fixed 64-record per-role arrays. It performs no allocation, formatting, file I/O, or locking. Records use `CLOCK_MONOTONIC_RAW` and are formatted as `RMG_RACE_V1` only after the race completes. `RMG_SCHED_V1` and `RMG_SYS_V1` snapshots are collected before arming and after completion. Missing optional files are reported with `available=0` and never affect exploit safety.

For controlled comparisons, keep race parameters unchanged between samples, use full kernel reboots, avoid manual intervention until Auto Root terminates, and export every successful or failed diagnostic report. Compare consumer-to-readiness and readiness-to-pselect-return timing, writer timing, CPU migration and involuntary-switch deltas, CPU frequency/state, PSI, thermal state, softirq/interrupt deltas, and allocator/reclaim counters. These are measurements and correlations, not causal classifications.
