# CZG3 race telemetry

Race telemetry remains part of the canonical `SM-S938B` CZG3 payload. The first production instrumentation was too invasive: it serialized race participants through a shared mutex and performed large `/proc`/sysfs snapshots while waiter/owner/consumer state was still timing-sensitive. On hardware that correlated with device-wide stalls and `RACE_STATE_UNCERTAIN` / `writer_state=NOT_ARMED` failures.

The corrected design keeps `RMG_DIAG_V1`, `RMG_RACE_V1`, `RMG_SCHED_V1`, and `RMG_SYS_V1` without doing text I/O or bulk `/proc` reads in the critical race window. Race events are written to fixed per-role buffers without a shared mutex. Scheduler/system snapshots are bounded and stored in memory. Text is emitted only from `czg3_race_dump()` after the race outcome is known.

`make TARGET=pa3q-S938BXXSBCZG3 release` therefore continues to build one canonical instrumented payload. `support/targets-v3.json` remains the only feed. There is no separate diagnostic payload channel.

The fail-closed `RACE_STATE_UNCERTAIN` behavior, writer-state policy, and keeper protections are independent of telemetry and must not be weakened to improve success rate. If instrumentation must change again, reduce its perturbation rather than deleting diagnostics or allowing retries over unknown kernel state.
