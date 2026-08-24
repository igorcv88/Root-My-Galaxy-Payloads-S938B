# Support feed schema

This fork keeps the legacy `targets-v2.json` feed for older released clients and an exact, hash-verified `targets-v3.json` feed for current S938B builds.

## Schema v3

Each payload entry contains:

- `payloadId` and `displayName`;
- one or more `Build.MODEL` values in `models`;
- one or more short kernel versions in `kernelVersions` for Advanced/manual catalog filtering;
- an `exactMatch` object used for automatic selection;
- `url`, `size`, and `sha256` for the exploit and KernelSU artifacts.

`models` and `kernelVersions` do **not** by themselves authorize automatic execution in this fork. They are compatibility metadata for the interactive Advanced flow.

Automatic selection is fail-closed and requires a complete `exactMatch` identity. For the maintained SM-S938B profile this includes:

- manufacturer;
- model;
- device;
- `Build.DISPLAY`;
- full build fingerprint;
- full `uname -r` kernel release;
- `uname` version information;
- `uname` machine;
- SDK level;
- ABI;
- page size.

If any exact identity field differs, the profile is not automatically selected.

## Artifact integrity

The application first resolves the current commit SHA of this controlled repository, then reads `support/targets-v3.json` from that immutable commit. Artifact URLs from the manifest are rewritten to the same commit before download.

Downloaded payloads must match both the declared byte size and SHA-256 digest before they are finalized for execution. Partial or mismatched downloads are deleted and fail closed.

The current S938B v3 payload is maintained separately from the generic upstream S25-series profile because the hardware-validated `pa3q-S938BXXSBCZG3` exploit is not byte-identical to the upstream generic S938N-derived exploit even though both files have the same release size.

## Legacy schema v2

`targets-v2.json` remains available for older fork builds that still parse schema version 2. New synchronized app builds read schema version 3.
