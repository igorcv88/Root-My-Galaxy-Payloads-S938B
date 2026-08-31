<p align="center">
  <img src=".github/assets/root-my-galaxy-payloads-banner.svg" alt="Root My Galaxy Payloads" width="100%" />
</p>

<p align="center">
  <a href="https://github.com/igorcv88/Root-My-Galaxy-S938B/releases/latest"><img alt="App release" src="https://img.shields.io/github/v/release/igorcv88/Root-My-Galaxy-S938B?label=app" /></a>
  <a href="https://github.com/igorcv88/Root-My-Galaxy-S938B/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/igorcv88/Root-My-Galaxy-S938B/total" /></a>
  <a href="https://github.com/igorcv88/Root-My-Galaxy-Payloads-S938B/stargazers"><img alt="Stars" src="https://img.shields.io/github/stars/igorcv88/Root-My-Galaxy-Payloads-S938B?label=stars&amp;labelColor=555&amp;color=2f81f7" /></a>
  <img alt="Firmware" src="https://img.shields.io/badge/firmware-S938BXXSBCZG3-59636e" />
  <img alt="KernelSU" src="https://img.shields.io/badge/KernelSU-3.3.0-2f81f7" />
  <a href="https://github.com/igorcv88/Root-My-Galaxy-Payloads-S938B/actions/workflows/update-payloads.yml"><img alt="Payload build" src="https://img.shields.io/github/actions/workflow/status/igorcv88/Root-My-Galaxy-Payloads-S938B/update-payloads.yml?branch=main&amp;label=payloads" /></a>
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

This repository contains the firmware profile and executable payloads maintained for the Root My Galaxy fork. If you only want to root the phone, install the APK from [Root My Galaxy](https://github.com/igorcv88/Root-My-Galaxy-S938B); these files do not need to be downloaded manually.

## Why this fork?

Compared with [upstream Root My Galaxy Payloads](https://github.com/BuSung-dev/Root-My-Galaxy-Payloads), this fork currently adds:

- **Exact `SM-S938B / S938BXXSBCZG3` support** for the maintained Galaxy S25 Ultra firmware.
- **KernelSU 3.3.0**, while upstream payloads are currently based on KernelSU 3.2.5.
- **SHA-256 verification** for the maintained CZG3 exploit and KernelSU payloads.
- **Active CZG3 reliability work**, including payload instrumentation used by Root My Galaxy's exploit diagnostics and latency analysis.

Upstream maintains a broader multi-device catalog. This fork currently focuses its automatic feed on the exact CZG3 profile above.

## Current profile

| | Configuration |
| --- | --- |
| Device | Samsung Galaxy S25 Ultra `SM-S938B` (`pa3q`) |
| Firmware | `S938BXXSBCZG3` |
| Android | Android 16 / API 36 |
| Kernel | `6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k` |
| ABI | `arm64-v8a` |
| Page size | 4K |

A firmware or kernel update can invalidate the exploit profile.

## What this repository provides

Root My Galaxy uses three pieces from this repository:

- **Support profile** — identifies the compatible device, firmware and kernel.
- **Exploit payload** — acquires temporary bootstrap root.
- **KernelSU payload** — late-loads the matching KernelSU build after bootstrap root is available.

The current app feed is stored in `support/targets-v3.json`. CZG3 executable entries include expected size and SHA-256 metadata.

## How Root My Galaxy uses the payloads

During a manual installation, Root My Galaxy downloads the matching payloads and verifies them before execution.

After a successful root, Auto Root keeps the verified working set locally. Publishing a newer payload does not break that existing Auto Root setup; a new set only replaces it after it has also completed a successful manual root on the device.

## Root lifetime

KernelSU is late-loaded for the current kernel boot. No boot image is flashed or modified. A full reboot returns the phone to its stock boot state, and Root My Galaxy can restore root through Auto Root.

## Integrity

The maintained CZG3 profile uses exact device identity plus size and SHA-256 checks for the executable payloads. If the device or files do not match the expected profile, Root My Galaxy stops instead of treating them as compatible.

The older v2 profile remains available for compatibility with previously released app versions.

## License and credits

This repository is distributed under the license in [LICENSE](LICENSE). It is derived from [BuSung-dev/Root-My-Galaxy-Payloads](https://github.com/BuSung-dev/Root-My-Galaxy-Payloads); the exploit work ultimately derives from the published CVE-2026-43499 research, and the late-load root payload uses [KernelSU](https://github.com/tiann/KernelSU).
