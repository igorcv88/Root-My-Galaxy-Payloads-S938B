# SM-S938B / S938BXXSBCZG3 port record

Status: runtime inventory captured; offline firmware analysis and target-specific builds pending.

This record intentionally does not declare the firmware supported. No target header,
P0 fingerprint table, exploit artifact, or KernelSU artifact should be published until
the offline gates below are complete.

## Device identity

```text
manufacturer: samsung
model: SM-S938B
device: pa3q
hardware: qcom
ABI: arm64-v8a
SDK: 36
Android: 16
page size: 4096
build display: BP4A.251205.006.S938BXXSBCZG3
build fingerprint: samsung/pa3qxxx/pa3q:16/BP4A.251205.006/S938BXXSBCZG3_OXMBCZG3:user/release-keys
bootloader: S938BXXSBCZG3
verified boot state: green
```

## Runtime kernel identity

```text
uname -r: 6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k
uname -v: #1 SMP PREEMPT Thu Jul  2 00:48:56 UTC 2026
/proc/version: Linux version 6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k (kleaf@build-host) (Android (11368308, +pgo, +bolt, +lto, +mlgo, based on r510928) clang version 18.0.0 (https://android.googlesource.com/toolchain/llvm-project 477610d4d0d988e69dbc3fae4fe86bff3f07f2b5), LLD 18.0.0) #1 SMP PREEMPT Thu Jul  2 00:48:56 UTC 2026
sched:sched_blocked_reason runtime event ID: 109
```

## Runtime module/configuration observations

The existing `pa3q-S938NKSUACZF1` advanced-mode selection was reported to
complete successfully on this device. At inventory time:

```text
kernelsu 172032 7 - Live 0x0000000000000000 (O)
kernel taint value: 5632
```

Relevant configuration values reported by `/proc/config.gz`:

```text
CONFIG_MODULES=y
CONFIG_MODULE_SIG=y
CONFIG_MODVERSIONS=y
CONFIG_MODULE_UNLOAD=y
CONFIG_KALLSYMS=y
CONFIG_KALLSYMS_ALL=y
CONFIG_TRIM_UNUSED_KSYMS=y
CONFIG_IKCONFIG=y
CONFIG_IKCONFIG_PROC=y
CONFIG_ARM64_4K_PAGES=y
CONFIG_SECURITY_DEFEX=y
CONFIG_RKP=y
CONFIG_KDP=y
```

The filtered runtime log contained normal KernelSU credential and syscall-hook
activity, including:

```text
KernelSU: Samsung KDP task-scoped credential install ... uid=0 euid=0
KernelSU: ksu fd installed ...
KernelSU: ksu fd released
```

The captured filtered log contained no literal `version magic`, `vermagic`,
`external abort`, or `synchronous abort` entries. This is evidence only for the
captured log window and is not proof of ABI compatibility.

## Preliminary upstream comparison

The checked-in `pa3q-S938NKSUACZF1` and `pa3q-S9380ZHUBCZF1` target headers
currently carry the same numeric exploit offsets and structure layout values,
while retaining firmware-specific build identity and separate P0 fingerprint
headers. That makes those profiles useful comparison baselines, not substitutes
for analysis of the CZG3 kernel image.

## Required offline gates

Before adding `pa3q-S938BXXSBCZG3` to `support/targets-v2.json`:

1. Record the exact firmware package identity and hashes.
2. Extract `boot.img.lz4`, the raw ARM64 kernel Image, and the relevant BL image.
3. Record boot image and kernel sizes and SHA-256 hashes.
4. Reconstruct a symbolized `vmlinux.elf` and validate the embedded BTF blob.
5. Derive every exploit symbol offset and every structure member offset from the
   CZG3 kernel rather than copying another PA3Q target.
6. Confirm physical load addresses from the CZG3 bootloader.
7. Derive the trace worker caller offset and verify it against runtime event ID
   109.
8. Generate and verify all P0 fingerprint rows from the CZG3 raw kernel Image.
9. Build a target-specific exploit payload and pass the repository offline gates.
10. Build KernelSU against the reconstructed target ELF/configuration, verify
    exact vermagic and symbol CRC requirements, and embed that module into a
    target-specific late-load binary.
11. Perform staged hardware testing and inspect complete kernel logs before the
    support-feed entry is enabled.

## Provenance still required

Attach or otherwise provide the exact downloaded firmware archives used for the
port, preferably the complete AP and BL tar.md5 files. Keep the raw extracted
kernel and a provenance note after analysis; temporary multi-gigabyte archives
and reconstructed intermediates can be removed only after hashes and derived
artifacts are recorded.
