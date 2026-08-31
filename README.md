<p align="center">
  <img src=".github/assets/root-my-galaxy-payloads-banner.svg" alt="Root My Galaxy Payloads" width="100%" />
</p>

<p align="center">
  <a href="https://github.com/igorcv88/Root-My-Galaxy-S938B/releases/latest"><img alt="App release" src="https://img.shields.io/github/v/release/igorcv88/Root-My-Galaxy-S938B?label=app" /></a>
  <a href="https://github.com/igorcv88/Root-My-Galaxy-S938B/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/igorcv88/Root-My-Galaxy-S938B/total" /></a>
  <a href="https://github.com/igorcv88/Root-My-Galaxy-Payloads-S938B/stargazers"><img alt="Stars" src="https://img.shields.io/github/stars/igorcv88/Root-My-Galaxy-Payloads-S938B" /></a>
  <img alt="Firmware" src="https://img.shields.io/badge/firmware-S938BXXSBCZG3-59636e" />
  <a href="https://github.com/igorcv88/Root-My-Galaxy-Payloads-S938B/actions/workflows/update-payloads.yml"><img alt="Payload build" src="https://img.shields.io/github/actions/workflow/status/igorcv88/Root-My-Galaxy-Payloads-S938B/update-payloads.yml?branch=main&label=payloads" /></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/igorcv88/Root-My-Galaxy-Payloads-S938B" /></a>
</p>

<p align="center">
  <strong>Payload and firmware-profile repository used by Root My Galaxy.</strong>
</p>

<p align="center">
  <a href="https://github.com/igorcv88/Root-My-Galaxy-S938B">Root My Galaxy app</a>
  ·
  <a href="https://github.com/BuSung-dev/Root-My-Galaxy-Payloads">Upstream payloads</a>
  ·
  <a href="https://github.com/igorcv88/Root-My-Galaxy-S938B/releases/latest">Latest APK</a>
</p>

This repository contains the firmware profile and executable payloads maintained for the Root My Galaxy fork. Normal users should install the APK from [Root My Galaxy](https://github.com/igorcv88/Root-My-Galaxy-S938B); they do not need to download or execute files from this repository manually.

## Why this fork?

This repository is derived from [BuSung-dev/Root-My-Galaxy-Payloads](https://github.com/BuSung-dev/Root-My-Galaxy-Payloads), but the current branch adds a separately maintained CZG3 path rather than simply mirroring upstream binaries.

The main differences are:

- **Exact `SM-S938B / S938BXXSBCZG3` profile** — the maintained v3 entry includes the complete device, build, kernel, ABI and page-size identity used by the app's fail-closed automatic selection.
- **Per-artifact SHA-256 metadata** — the CZG3 exploit and KernelSU userspace payload are checked by both size and SHA-256 before the app treats them as valid.
- **CZG3-specific KernelSU 3.3.0 build** — the repository maintains the Samsung KDP/RKP/DEFEX late-load module and `ksud` pair used by this fork, including the staged-daemon handoff fix required by the current CZG3 path.
- **Controlled payload rebuild workflow** — exploit and/or KernelSU can be rebuilt from one manual `Atualizar Payloads` workflow, which recalculates metadata, validates the result, commits only real changes, and can then trigger a signed Root My Galaxy APK release.
- **Auto Root snapshot compatibility** — the companion app no longer depends on the latest repository payload at boot. A manually verified payload set becomes a local known-good snapshot, so publishing a newer payload does not invalidate an already working Auto Root installation.

The upstream repository contains a broader multi-device payload catalog. This fork still carries upstream history and source, but its **current controlled v3 feed is intentionally focused on the exact CZG3 profile** while broader catalog delivery is separated from the working S25 Ultra safety path.

## Current controlled profile

| | Configuration |
| --- | --- |
| Device | Samsung Galaxy S25 Ultra `SM-S938B` (`pa3q`) |
| Firmware | `S938BXXSBCZG3` |
| Android | Android 16 / API 36 |
| Kernel | `6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k` |
| ABI | `arm64-v8a` |
| Page size | 4K |

A firmware or kernel update can invalidate the current exploit profile. Root My Galaxy compares the device against the maintained identity before automatic selection.

## What this repository provides

Root My Galaxy consumes three pieces from this repository:

- **Support profile** — describes which device, firmware and kernel an exact payload belongs to.
- **Exploit payload** — performs the supported kernel exploit and acquires temporary bootstrap root.
- **KernelSU payload** — provides the late-load KernelSU userspace/module chain after bootstrap root is available.

The active feed is stored in `support/targets-v3.json`. The maintained CZG3 executable entries include expected file size and SHA-256 checksum.

## How Root My Galaxy uses these payloads

During a normal manual installation, Root My Galaxy resolves the current payload repository commit, loads the support profile, downloads the files from that immutable commit and verifies their declared metadata before execution.

New downloads are placed in a **pending** area. They do not immediately replace the payloads available to Auto Root.

After the manual exploit and KernelSU late-load complete successfully, the app's existing success receipt identifies that pending set as proven on the device. On the next full boot, Root My Galaxy promotes that exact set into a durable, versioned local Auto Root snapshot and verifies it again before execution.

Consequently:

```text
repository publishes payload B
        ↓
user already has verified payload A locally
        ↓
phone reboots before user opens the app
        ↓
Auto Root still uses verified payload A
```

A newer payload becomes authoritative for Auto Root only after a successful manual installation proves it. Failed update attempts cannot overwrite the previous known-good snapshot.

The APK still embeds a single exact CZG3 profile as a compatibility fallback for users migrating from app versions that predate durable snapshots. It is no longer the long-term source of truth once a local snapshot exists.

## Root lifetime

The payload late-loads KernelSU for the current kernel boot. It does not flash a boot image or make the Samsung kernel modification permanent. A full reboot returns the device to the normal stock boot state; Root My Galaxy can then restore KernelSU through its optional Auto Root feature.

## Integrity and compatibility

The CZG3 profile uses exact device identity plus checksums for the exploit and KernelSU payload. If the profile does not match the device or a file does not match its declared size/SHA-256, Root My Galaxy stops instead of treating it as valid.

The older v2 profile remains present for compatibility with previously released app versions.

## Workflow

The repository intentionally exposes only one GitHub Actions workflow: **Atualizar Payloads**.

It can rebuild `Exploit`, `KernelSU`, or both. Validation runs inside that same workflow before anything is pushed. When files actually change, the workflow commits them to `main` and, by default, dispatches **Gerar APK Release** in the Root My Galaxy repository.

If a rebuild produces byte-identical output, no commit is created and no unnecessary APK release is triggered.

## License and credits

This repository is distributed under the license in [LICENSE](LICENSE). It is derived from [BuSung-dev/Root-My-Galaxy-Payloads](https://github.com/BuSung-dev/Root-My-Galaxy-Payloads); the exploit work ultimately derives from the published CVE-2026-43499 research, and the late-load root payload uses [KernelSU](https://github.com/tiann/KernelSU).
