# Root My Galaxy Payloads — S938B

This repository is the maintained payload and support-feed companion for
[Root My Galaxy S938B](https://github.com/igorcv88/Root-My-Galaxy-S938B).
The Android application resolves the current `main` commit first, then downloads
the manifest and native files from that immutable commit.

It contains:

- exact firmware profiles and offsets;
- the app-domain CVE-2026-43499 native exploit payload;
- KernelSU late-load binaries;
- `support/targets-v2.json`, consumed by the application.

It intentionally does not contain the Android application source or a Zygisk
implementation.

## Validated target

| Profile | Device | Firmware | Kernel | Status |
| --- | --- | --- | --- | --- |
| `pa3q-S938BXXSBCZG3` | Galaxy S25 Ultra `SM-S938B` | `BP4A.251205.006.S938BXXSBCZG3` | `6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k` | Hardware validated |

The validated device acquired temporary KernelSU root with the simple exploit.
After an external KernelSU Manager **Soft Reboot**, Zygisk modules worked with
both a compatible provider and the tested
[NeoZygisk PostBoot fork](https://github.com/igorcv88/NeoZygisk-PostBoot).

## Other inherited profiles

The feed also retains the upstream profiles below. They are not equivalent to
the validated S938B CZG3 target and must not be selected automatically for a
different build.

| Profile | Device | Firmware | Status |
| --- | --- | --- | --- |
| `pa3q-S938NKSUACZF1` | `SM-S938N` | `BP4A.251205.006.S938NKSUACZF1` | Upstream profile |
| `pa3q-S9380ZHUBCZF1` | `SM-S9380` | `BP4A.251205.006.S9380ZHUBCZF1` | Upstream profile |
| `e3q-S928USQS6DZF2` | `SM-S928U` | `BP4A.251205.006.S928USQS6DZF2` | Experimental/upstream |

Profiles are exact-firmware profiles. The application matches the full kernel
release, full build display ID, SDK, ABI and page size. A matching model with a
different firmware is not a supported equivalent.

## Feed integrity model

The application is configured specifically for:

```text
https://github.com/igorcv88/Root-My-Galaxy-Payloads-S938B
```

It obtains the SHA of `refs/heads/main`, then rewrites every mutable manifest URL
to the same immutable commit. The validation workflow rejects:

- artifact URLs outside this repository;
- missing files;
- file-size mismatches;
- a missing or altered `pa3q-S938BXXSBCZG3` profile;
- duplicate profile IDs.

Schema version 2 records declared file sizes but does not include per-file
SHA-256 values or a signed manifest. Commit pinning prevents a manifest and its
payloads from being fetched from different revisions.

## Building a native exploit payload

```sh
make TARGET=pa3q-S938BXXSBCZG3 ANDROID_NDK_HOME=/path/to/android-ndk release
```

Expected outputs:

```text
build/<profile>/cve-2026-43499
build/<profile>/cve-2026-43499-app.so
build/<profile>/cve-2026-43499-root
```

The original exploit work is derived from NebuSec's CVE-2026-43499 research.
Use this repository only on devices you own or are explicitly authorized to
test.
