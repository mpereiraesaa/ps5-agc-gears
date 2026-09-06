#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

from test_validate_texture_path_accounting_evidence import BUNDLE_SHA, make_evidence


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_texture_path_final_evidence.py"


def make_final_evidence(directory: Path) -> Path:
    path = make_evidence(directory)
    manifest = json.loads(path.read_text())
    log = directory / manifest["log_path"]
    lines = log.read_text().splitlines()
    messages = [line.split("\t", 3)[3] for line in lines[1:-1]]
    replacements = (
        ("slice=accounting", "slice=final"),
        ("mode=texture-path-accounting-soak", "mode=texture-path-final-soak"),
        ("frame=9998", "frame=59998"),
        ("frame=9999", "frame=59999"),
        ("frames=10000", "frames=60000"),
        ("completed=10000", "completed=60000"),
        ("bounded_upload_frames=9998", "bounded_upload_frames=59998"),
        ("cumulative_transient_bytes=9999000", "cumulative_transient_bytes=59999000"),
        ("cumulative_lightmap_bytes=2579232", "cumulative_lightmap_bytes=15379232"),
        ("cumulative_bytes=12578232", "cumulative_bytes=75378232"),
        ("total_uploaded_bytes=12578232", "total_uploaded_bytes=75378232"),
        ("transient_bytes_total=10000000", "transient_bytes_total=60000000"),
        ("cumulative_transient_bytes=10000000", "cumulative_transient_bytes=60000000"),
        ("lightmap_bytes_total=2579488", "lightmap_bytes_total=15379488"),
        ("cumulative_lightmap_bytes=2579488", "cumulative_lightmap_bytes=15379488"),
        ("upload_bytes_total=12579488", "upload_bytes_total=75379488"),
        ("uploaded_bytes=12579488", "uploaded_bytes=75379488"),
        ("cumulative_bytes=12579488", "cumulative_bytes=75379488"),
        ("total_uploaded_bytes=12579488", "total_uploaded_bytes=75379488"),
    )
    converted = []
    for message in messages:
        for old, new in replacements:
            message = message.replace(old, new)
        converted.append(message)
    final = (
        "BSP_TEXTURE_PATH_FINAL_COMPLETE schema=1 frames=60000 "
        "mip_chains=10 opaque_draws=80 alpha_draws=10 sky_draws=10 "
        "pipelines=4 sampler=anisotropic4x dynamic_lightmap=bounded "
        "pool_resident_bytes=141024 texture_payload_bytes=60000 "
        "upload_bytes_total=75379488 sequence_hash=4444444444444444 "
        "readback=gpu-visible input_dependency=none tokens=exact "
        "guards=intact errors=0"
    )
    converted.insert(-1, final)
    records = [f"{index}\t{index}\tMARK\t{message}"
               for index, message in enumerate(converted, 1)]
    reason = "bsp-texture-path-final-soak-complete"
    payload = "\n".join([
        lines[0], *records, f"BYE seq={len(records)} reason={reason}", "",
    ]).encode()
    log.write_bytes(payload)
    manifest["records"] = len(records)
    manifest["last_seq"] = len(records)
    manifest["bye_fields"] = {"seq": str(len(records)), "reason": reason}
    manifest["bytes"] = len(payload)
    manifest["sha256"] = hashlib.sha256(payload).hexdigest()
    path.write_text(json.dumps(manifest))
    return path


def run(path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["python3", str(VALIDATOR), str(path), "--bundle-sha256", BUNDLE_SHA,
         "--bundle-bytes", "42"], text=True, capture_output=True, check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        path = make_final_evidence(directory)
        valid = run(path)
        assert valid.returncode == 0, valid.stderr
        summary = json.loads(valid.stdout)
        assert summary["frames"] == 60_000
        assert summary["upload_bytes_total"] == 75_379_488

        log = directory / "synthetic.log"
        log.write_bytes(log.read_bytes().replace(
            b"BSP_TEXTURE_PATH_FINAL_COMPLETE schema=1 frames=60000",
            b"BSP_TEXTURE_PATH_FINAL_COMPLETE schema=1 frames=59999",
        ))
        manifest = json.loads(path.read_text())
        payload = log.read_bytes()
        manifest["bytes"] = len(payload)
        manifest["sha256"] = hashlib.sha256(payload).hexdigest()
        path.write_text(json.dumps(manifest))
        rejected = run(path)
        assert rejected.returncode != 0 and "final Phase 3" in rejected.stderr
    print("final texture-path evidence validator tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
