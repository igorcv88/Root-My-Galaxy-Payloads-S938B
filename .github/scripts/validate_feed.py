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
LEGACY_EXPLOIT = pathlib.Path(
    "artifacts/pa3q-S938BXXSBCZG3/cve-2026-43499-app.so"
)
V3_EXPLOIT = pathlib.Path(
    "artifacts/pa3q-S938BXXSBCZG3-v0265/cve-2026-43499-app.so"
)
LEGACY_EXPLOIT_SHA256 = (
    "ba0894d1214e3c46305d8acb0ab065eb110833b4b9973c9250aca5bfcb98c214"
)
V3_EXPLOIT_SHA256 = (
    "1719e9362cd19e58521cb785fcaa40c4613ca854d0c3c9fb8320edf8e9046303"
)
V3_KERNELSU_SHA256 = (
    "5a009f1fc58b25a6e197d8ec951a86ec11a9b5ec9d56bdb5f7a3410a22b9b48a"
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


def validate_v3_artifact(artifact: dict) -> pathlib.Path:
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
    assert len(v2.get("targets", [])) == 1, "legacy v2 feed must remain S938B-only"
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

    legacy_exploit = local_path(legacy["exploit"]["url"])
    assert legacy_exploit == ROOT / LEGACY_EXPLOIT
    assert legacy_exploit.stat().st_size == legacy["exploit"]["size"] == 104128
    assert sha256(legacy_exploit) == LEGACY_EXPLOIT_SHA256, (
        "legacy hardware-validated v2 exploit changed"
    )
    legacy_ksud = local_path(legacy["kernelsu"]["url"])
    assert legacy_ksud.is_file()
    assert legacy_ksud.stat().st_size == legacy["kernelsu"]["size"] == 6407096

    v3 = json.loads((ROOT / "support/targets-v3.json").read_text(encoding="utf-8"))
    assert v3.get("schemaVersion") == 3
    payloads = v3.get("payloads")
    assert isinstance(payloads, list) and len(payloads) == 1, "v3 feed must remain S938B-only"
    target = payloads[0]
    assert target["payloadId"] == "pa3q-S938BXXSBCZG3"
    assert target["models"] == ["SM-S938B"]
    assert target["kernelVersions"] == ["6.6.98"]
    assert target["exactMatch"] == EXPECTED_IDENTITY, "exact S938B identity drifted"

    exploit = validate_v3_artifact(target["exploit"])
    validate_v3_artifact(target["kernelsu"])
    assert exploit == ROOT / V3_EXPLOIT, "v3 must keep the restored v0265 payload"
    assert target["exploit"]["sha256"] == V3_EXPLOIT_SHA256
    assert target["kernelsu"]["sha256"] == V3_KERNELSU_SHA256
    assert exploit.read_bytes() != legacy_exploit.read_bytes(), (
        "v2 legacy and v3 restored payloads unexpectedly collapsed"
    )

    assert (ROOT / "src/targets/pa3q-S938BXXSBCZG3/target.h").is_file()
    assert (ROOT / "src/targets/pa3q-S938BXXSBCZG3/p0_fingerprint.h").is_file()
    print("Payload feed is valid (immutable legacy v2 + restored minimal v3 CZG3)")


if __name__ == "__main__":
    main()
