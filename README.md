# Root My Galaxy Payloads — SM-S938B

This repository is the controlled payload feed for [`igorcv88/Root-My-Galaxy-S938B`](https://github.com/igorcv88/Root-My-Galaxy-S938B).

Its exploit infrastructure is synchronized with [`BuSung-dev/Root-My-Galaxy-Payloads`](https://github.com/BuSung-dev/Root-My-Galaxy-Payloads), while the application feed remains deliberately restricted to the exact validated Galaxy S25 Ultra firmware used by this fork.

## Executable feed profile

`support/targets-v3.json` exposes one fail-closed profile:

```text
Payload ID:   pa3q-S938BXXSBCZG3
Model:        SM-S938B
Device:       pa3q
Build:        BP4A.251205.006.S938BXXSBCZG3
Fingerprint:  samsung/pa3qxxx/pa3q:16/BP4A.251205.006/S938BXXSBCZG3_OXMBCZG3:user/release-keys
Kernel:       6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k
uname -v:     #1 SMP PREEMPT Thu Jul  2 00:48:56 UTC 2026
Machine:      aarch64
SDK / ABI:    36 / arm64-v8a
Page size:    4096
```

The exact target source is under `src/targets/pa3q-S938BXXSBCZG3/`. Its CZG3 P0 fingerprint table is not byte-identical to the S938N CZF1 table, so the shared upstream S25 profile is not substituted for this exact target.

## Legacy v2 compatibility

`support/targets-v2.json` is intentionally retained for already released versions of the S938B app. It contains only the exact CZG3 profile and continues to reference the previously hardware-validated exploit:

```text
artifacts/pa3q-S938BXXSBCZG3/cve-2026-43499-app.so
  size:    104128
  sha256:  ba0894d1214e3c46305d8acb0ab065eb110833b4b9973c9250aca5bfcb98c214
```

That path must not be replaced by a newly rebuilt payload. The validation workflow checks its SHA-256 even though the legacy v2 manifest itself predates hash fields.

## v3 synchronized payload

The v3 app uses a separate artifact rebuilt from the exact CZG3 target against the synchronized upstream exploit source:

```text
artifacts/pa3q-S938BXXSBCZG3-v0265/cve-2026-43499-app.so
  size:    104128
  sha256:  1719e9362cd19e58521cb785fcaa40c4613ca854d0c3c9fb8320edf8e9046303

kernelsu/ksud-s25u-kdp
  size:    6407096
  sha256:  fa3edcc7d168637394877b30cb1f909d762dda788ec14051f4ae79edd6562d63
```

The v3 feed includes size and SHA-256 for every executable artifact. The application resolves this repository's current commit first, then rewrites all manifest artifact URLs to that immutable commit before downloading them.

The `Validate payload feed` workflow rejects external URLs, missing files, size/hash mismatches, identity drift, changes to the legacy hardware-validated payload, or accidental collapse of the v2 and v3 generations.

## Upstream synchronization

The repository also keeps the current upstream exploit source, target infrastructure, KernelSU build material, and additional device research. Those additional sources are not automatically eligible for execution by the S938B application feed unless explicitly added to the controlled manifest with exact identity and hashes.

## Build

For the S938B target:

```sh
make TARGET=pa3q-S938BXXSBCZG3 ANDROID_NDK_HOME=/path/to/android-ndk release
```

Outputs are generated under `build/pa3q-S938BXXSBCZG3/`.

The firmware analysis and target derivation are recorded in [`docs/SM-S938B-S938BXXSBCZG3.md`](docs/SM-S938B-S938BXXSBCZG3.md). General upstream-derived porting material remains under `docs/` and `kernelsu/`.

Use only on devices you own or are explicitly authorized to test.
