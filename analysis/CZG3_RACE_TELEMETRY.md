# CZG3 race telemetry

Race telemetry was tested on the `SM-S938B` CZG3 physical-write path and was found to perturb the scheduler-sensitive exploit on real hardware. The instrumented build produced device-wide stalls and repeated `RACE_STATE_UNCERTAIN` / `writer_state=NOT_ARMED` failures. For that reason, the canonical production payload no longer enables `CZG3_RACE_TELEMETRY`.

`make TARGET=pa3q-S938BXXSBCZG3 release` builds the uninstrumented, fixed-size production payload. `support/targets-v3.json` points only to that canonical payload; there is no parallel diagnostic feed or diagnostic payload channel.

Any future race telemetry work must remain source-only until it is proven not to alter the physical race on-device. In particular, production instrumentation must not add `/proc` or sysfs snapshots, large stdout bursts, scheduler queries, mutexes, extra syscalls, or synchronization to the critical waiter/owner/consumer path.
