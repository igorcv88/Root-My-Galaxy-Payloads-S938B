#!/usr/bin/env python3
import hashlib
import json
import pathlib
import re
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parents[2]
PREFIX = (
    "https://raw.githubusercontent.com/igorcv88/"
    "Root-My-Galaxy-Payloads-S938B/main/"
)
CANONICAL_EXPLOIT = pathlib.Path(
    "artifacts/pa3q-S938BXXSBCZG3/cve-2026-43499-app.so"
)
EXPECTED_IDENTITY = {
    "manufacturer": "samsung",
    "model": "SM-S938B",
    "device": "pa3q",
    "buildDisplay": "BP4A.251205.006.S938BXXSBCZG3",
    "buildFingerprint": (
        "samsung/pa3qxxx/pa3q:16/BP4A.251205.006/"
        "S938BXXSBCZG3_OXMBCZG3:user/release-keys"
    ),
    "kernelRelease": "6.6.98-android15-8-pd6ff1cd-abogkiS938BXXSBCZG3-4k",
    "kernelVersionInfo": "#1 SMP PREEMPT Thu Jul  2 00:48:56 UTC 2026",
    "machine": "aarch64",
    "sdk": 36,
    "abi": "arm64-v8a",
    "pageSize": 4096,
}


def local_path(url: str) -> pathlib.Path:
    if not url.startswith(PREFIX):
        raise AssertionError(f"external artifact URL: {url}")
    return ROOT / urllib.parse.unquote(url.removeprefix(PREFIX))


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def validate_artifact(artifact: dict) -> pathlib.Path:
    expected_sha = artifact["sha256"].lower()
    if not re.fullmatch(r"[0-9a-f]{64}", expected_sha):
        raise AssertionError("invalid SHA-256 in CZG3 v3 manifest entry")
    path = local_path(artifact["url"])
    if not path.is_file():
        raise AssertionError(f"missing {path.relative_to(ROOT)}")
    if path.stat().st_size != artifact["size"]:
        raise AssertionError(f"size mismatch for {path.relative_to(ROOT)}")
    if sha256(path) != expected_sha:
        raise AssertionError(f"SHA-256 mismatch for {path.relative_to(ROOT)}")
    return path


def main() -> None:
    v2 = json.loads((ROOT / "support/targets-v2.json").read_text(encoding="utf-8"))
    assert v2.get("schemaVersion") == 2
    assert len(v2.get("targets", [])) == 1, "v2 feed must remain S938B-only"
    legacy = v2["targets"][0]
    assert legacy["profileId"] == "pa3q-S938BXXSBCZG3"
    assert legacy["manufacturer"] == EXPECTED_IDENTITY["manufacturer"]
    assert legacy["model"] == EXPECTED_IDENTITY["model"]
    assert legacy["device"] == EXPECTED_IDENTITY["device"]
    assert legacy["buildDisplay"] == EXPECTED_IDENTITY["buildDisplay"]
    assert legacy["buildFingerprint"] == EXPECTED_IDENTITY["buildFingerprint"]
    assert legacy["kernelRelease"] == EXPECTED_IDENTITY["kernelRelease"]
    assert legacy["kernelBuildVersion"] == EXPECTED_IDENTITY["kernelVersionInfo"]
    assert legacy["sdk"] == EXPECTED_IDENTITY["sdk"]
    assert legacy["abi"] == EXPECTED_IDENTITY["abi"]
    assert legacy["pageSize"] == EXPECTED_IDENTITY["pageSize"]

    v3 = json.loads((ROOT / "support/targets-v3.json").read_text(encoding="utf-8"))
    assert v3.get("schemaVersion") == 3
    payloads = v3.get("payloads")
    assert isinstance(payloads, list) and payloads, "v3 feed must contain payloads"

    ids = [payload.get("payloadId") for payload in payloads]
    assert all(isinstance(payload_id, str) and payload_id for payload_id in ids)
    assert len(ids) == len(set(ids)), "v3 payloadIds must be unique"

    matches = [
        payload for payload in payloads
        if payload.get("payloadId") == "pa3q-S938BXXSBCZG3"
    ]
    assert len(matches) == 1, "exact CZG3 payload must exist exactly once"
    target = matches[0]
    assert target["models"] == ["SM-S938B"]
    assert target["kernelVersions"] == ["6.6.98"]
    assert target["exactMatch"] == EXPECTED_IDENTITY, "exact S938B identity drifted"

    exploit = validate_artifact(target["exploit"])
    validate_artifact(target["kernelsu"])

    expected_path = ROOT / CANONICAL_EXPLOIT
    assert exploit == expected_path, "v3 exploit must use the single canonical main path"
    legacy_exploit = local_path(legacy["exploit"]["url"])
    assert legacy_exploit == expected_path, "v2 and v3 must share the canonical exploit"
    assert legacy["exploit"]["size"] == target["exploit"]["size"] == exploit.stat().st_size

    # A source-only PR can intentionally lead the committed canonical binary.
    # Release instrumentation properties are validated against the freshly built
    # payload in update-payloads.yml, not against a potentially lagging artifact.

    versioned = sorted((ROOT / "artifacts").glob("pa3q-S938BXXSBCZG3-v*"))
    assert not versioned, (
        "versioned CZG3 artifacts are forbidden; use only "
        f"{CANONICAL_EXPLOIT.as_posix()}"
    )

    assert (ROOT / "src/targets/pa3q-S938BXXSBCZG3/target.h").is_file()
    assert (ROOT / "src/targets/pa3q-S938BXXSBCZG3/p0_fingerprint.h").is_file()
    print(
        "Payload feed is valid "
        f"({len(payloads)} profile(s); one canonical CZG3 payload on main)"
    )


if __name__ == "__main__":
    main()
