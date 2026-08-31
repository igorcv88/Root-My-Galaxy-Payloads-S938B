# SM-S938B CZG3 KernelSU v3.3.0 late-load contract

The SM-S938B/CZG3 Android 16 bootstrap-root path must not stage the running `ksud` by copying `/proc/self/exe` into `/data/adb/ksud` before the KernelSU module is loaded. On this Samsung target that read/copy can be rejected with `EPERM` by the pre-KernelSU security environment.

Root My Galaxy therefore uploads the already verified `ksud` payload to `/data/local/tmp/.ksud-stage` before invoking late-load. The CZG3 v3.3.0 userspace hotfix consumes that file with `stage_daemon_from()`, atomically renaming it to `/data/adb/ksud`, then applying root ownership and mode `0755`. This restores the handoff used by the known-working Samsung v3.2.5 path without changing the official KernelSU v3.3.0 version count.

Current rebuilt CZG3 artifacts:

- `android15-6.6_kernelsu-s25u-kdp-v3.3.0.ko`: 332416 bytes, SHA-256 `49ea9b561e29dd4f73d626c76978ac5b87d4bbd8f43b607e3b43186385875d8d`
- `ksud-s25u-kdp-v3.3.0`: 5096104 bytes, SHA-256 `5a009f1fc58b25a6e197d8ec951a86ec11a9b5ec9d56bdb5f7a3410a22b9b48a`

The `ksud` artifact must stay synchronized with `support/targets-v3.json` and with the app's embedded Auto Root manifest.