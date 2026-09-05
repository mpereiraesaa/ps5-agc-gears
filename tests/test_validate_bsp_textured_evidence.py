#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_bsp_textured_evidence.py"
BUNDLE_SHA = "1" * 64


def make_evidence(directory: Path) -> Path:
    messages = [
        "LOG_BOOT_MONOTONIC_NS=0x1234",
        f"BSP_TEXTURED_BOOT schema=1 target=gfx1013 fw=test composition=base_x_lightmap bundle_sha256={BUNDLE_SHA} bundle_bytes=42 soak_frames=60000",
        "BSP_NOCLIP_PAD_READY sticks=dual triggers=vertical connected_required=true",
        "BSP_TEXTURE_TABLES_READY textures=164 descriptor_dwords=3936 lightmap=512x799 base_mode=repeat lightmap_mode=clamp filter=bilinear composition=base_x_lightmap",
        "BSP_LOOP_BEGIN mode=textured-noclip-soak buffers=2 color_dma=false depth_dma=true indexed=true frames=60000 composition=base_x_lightmap",
        "BSP_VIDEOOUT_TOKEN frame=0 buffer=0 expected=100 observed=100 exact=true",
    ]
    for frame in range(599, 60_000, 600):
        connected = frame + 1
        messages.append(
            "BSP_NOCLIP_INPUT "
            f"frame={frame} connected={connected} moving={connected} looking={connected} "
            f"distance_milli={connected * 1000} changes={connected} hash={'2' * 16} read_errors=0"
        )
    messages += [
        "BSP_VIDEOOUT_TOKEN frame=59999 buffer=1 expected=101 observed=101 exact=true",
        "BSP_FRAME frame=59999 completed=60000 compose_avg_ns=1 gpu_wait_avg_ns=1 "
        "video_wait_avg_ns=1 present_interval_avg_ns=1 present_interval_max_ns=1 "
        "present_interval_over_budget=0 errors=0 terminal=0",
        "BSP_TEXTURED_READBACK buffer0=3333333333333333 buffer1=5555555555555555 "
        "bytes=64 bright_pixels0=1 bright_pixels1=1 guards=intact frames=60000 errors=0",
        "BSP_TEXTURED_SOAK_COMPLETE frames=60000 connected_frames=60000 moving_frames=60000 "
        "looking_frames=60000 distance_milli=60000000 input_changes=60000 "
        "input_hash=4444444444444444 read_errors=0 textures=164 descriptor_dwords=3936 "
        "composition=base_x_lightmap tokens=exact guards=intact errors=0",
    ]
    records = [f"{index}\t{index}\tMARK\t{message}" for index, message in enumerate(messages, 1)]
    log_name = "synthetic.log"
    transcript = "\n".join(
        ["HELLO ps5log/1 title=PPSA99997 app=ps5-agc-gears boot=0x1234 tag=test"]
        + records
        + [f"BYE seq={len(records)} reason=bsp-textured-soak-complete", ""]
    ).encode()
    (directory / log_name).write_bytes(transcript)
    manifest = {
        "identity": {"title": "PPSA99997", "app": "ps5-agc-gears", "boot": "0x1234"},
        "protocol": "ps5log/1",
        "transport": "tcp",
        "hello": True,
        "bye": True,
        "clean": True,
        "gaps": [],
        "raw_lines": 0,
        "oversized_lines": 0,
        "records": len(records),
        "last_seq": len(records),
        "bye_fields": {"seq": str(len(records)), "reason": "bsp-textured-soak-complete"},
        "log_path": log_name,
        "bytes": len(transcript),
        "sha256": hashlib.sha256(transcript).hexdigest(),
        "run_id": "synthetic",
    }
    manifest_path = directory / "synthetic.json"
    manifest_path.write_text(json.dumps(manifest))
    return manifest_path


def run_validator(manifest: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            "python3",
            str(VALIDATOR),
            str(manifest),
            "--bundle-sha256",
            BUNDLE_SHA,
            "--bundle-bytes",
            "42",
        ],
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        manifest = make_evidence(directory)
        valid = run_validator(manifest)
        assert valid.returncode == 0, valid.stderr
        summary = json.loads(valid.stdout)
        assert summary["frames"] == 60_000
        assert summary["textures"] == 164

        data = json.loads(manifest.read_text())
        log_path = directory / data["log_path"]
        original = log_path.read_bytes()
        log_path.write_bytes(original.replace(b"descriptor_dwords=3936", b"descriptor_dwords=3935", 1))
        changed = log_path.read_bytes()
        data["bytes"] = len(changed)
        data["sha256"] = hashlib.sha256(changed).hexdigest()
        manifest.write_text(json.dumps(data))
        semantic_failure = run_validator(manifest)
        assert semantic_failure.returncode != 0
        assert "cardinality" in semantic_failure.stderr

        manifest = make_evidence(directory)
        log_path = directory / json.loads(manifest.read_text())["log_path"]
        with log_path.open("ab") as handle:
            handle.write(b"tamper")
        integrity_failure = run_validator(manifest)
        assert integrity_failure.returncode != 0
        assert "size/hash" in integrity_failure.stderr
    print("bsp textured evidence validator tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
