#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_bsp_resource_evidence.py"
BUNDLE_SHA = "1" * 64


def make_evidence(directory: Path) -> Path:
    messages = [
        "LOG_BOOT_MONOTONIC_NS=0x1234",
        f"BSP_RESOURCE_BOOT schema=1 target=gfx1013 fw=test constants=vsharp transient_slots=2 pipelines=2 overlay=quad bundle_sha256={BUNDLE_SHA} bundle_bytes=42 soak_frames=60000",
        "BSP_NOCLIP_PAD_READY sticks=dual triggers=vertical connected_required=true",
        "BSP_BUNDLE_READY vertices=16893 indices=29013 map_draws=3611 scene_draws=3612 draw_dwords=1 command_slot_bytes=1 allocation_bytes=1",
        "RESOURCE_HEAP_READY bytes=20000000 allocations=4 pool=fence-retired transient_slots=2 slot_bytes=131072",
        "BSP_TEXTURE_TABLE_LAYOUT textures=164 descriptor_dwords=3936 storage=per-frame-transient lightmap=512x760 base_mode=repeat lightmap_mode=clamp filter=bilinear composition=base_x_lightmap",
        "RESOURCE_PIPELINES_READY count=2 map=bsp_resource overlay=bsp_overlay map_gs_words=2 overlay_gs_words=1 overlay_depth=disabled",
        "BSP_LOOP_BEGIN mode=resource-foundation-soak buffers=2 color_dma=false depth_dma=true indexed=true frames=60000 constants=per-frame descriptors=per-frame overlay=transient retirement=fence+videoout",
        "RESOURCE_FRAME_READY frame=0 slot=0 transient_bytes=95360 constant_dwords=32 texture_descriptor_dwords=3936 overlay_vertices=4 overlay_source=constant-vsharp overlay_indices=6 acquire_engine=1 gcr=00009000 poll_cycles=190 tables_gpu_span=true",
        "RESOURCE_FRAME_SEALED frame=0 slot=0 token=100 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=0 slot=0 token=100 command_dwords=155",
        "RESOURCE_FRAME_RETIRED frame=0 slot=0 token=100 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=0 buffer=0 expected=100 observed=100 exact=true",
        "RESOURCE_FRAME_READY frame=59999 slot=1 transient_bytes=95360 constant_dwords=32 texture_descriptor_dwords=3936 overlay_vertices=4 overlay_source=constant-vsharp overlay_indices=6 acquire_engine=1 gcr=00009000 poll_cycles=190 tables_gpu_span=true",
        "RESOURCE_FRAME_SEALED frame=59999 slot=1 token=60099 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=59999 slot=1 token=60099 command_dwords=155",
        "RESOURCE_FRAME_RETIRED frame=59999 slot=1 token=60099 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=59999 buffer=1 expected=60099 observed=60099 exact=true",
        "BSP_FRAME frame=59999 completed=60000 compose_avg_ns=1 gpu_wait_avg_ns=1 video_wait_avg_ns=1 present_interval_avg_ns=1 present_interval_max_ns=1 present_interval_over_budget=0 errors=0 terminal=0",
        "RESOURCE_RING_RETIRED slots=2 reusable=true tokens=exact last_token=60099",
        "BSP_RESOURCE_READBACK buffer0=3333333333333333 buffer1=5555555555555555 bytes=64 bright_pixels0=1 bright_pixels1=1 guards=intact frames=60000 errors=0 overlay=transient",
        "BSP_RESOURCE_SOAK_COMPLETE frames=60000 connected_frames=60000 read_errors=0 textures=164 descriptor_dwords=3936 constants=per-frame overlay=transient pipelines=2 tokens=exact guards=intact errors=0",
        "RESOURCE_POOL_RETIRED token=60099 reclaimed=4 completion=fence+videoout",
    ]
    records = [f"{index}\t{index}\tMARK\t{message}"
               for index, message in enumerate(messages, 1)]
    transcript = "\n".join(
        ["HELLO ps5log/1 title=PPSA99997 app=ps5-agc-gears boot=0x1234 tag=test"]
        + records
        + [f"BYE seq={len(records)} reason=bsp-resource-soak-complete", ""]
    ).encode()
    log_name = "synthetic.log"
    (directory / log_name).write_bytes(transcript)
    manifest = {
        "identity": {"title": "PPSA99997", "app": "ps5-agc-gears",
                     "boot": "0x1234"},
        "protocol": "ps5log/1", "transport": "tcp",
        "hello": True, "bye": True, "clean": True,
        "gaps": [], "raw_lines": 0, "oversized_lines": 0,
        "records": len(records), "last_seq": len(records),
        "bye_fields": {"seq": str(len(records)),
                       "reason": "bsp-resource-soak-complete"},
        "log_path": log_name, "bytes": len(transcript),
        "sha256": hashlib.sha256(transcript).hexdigest(),
        "run_id": "synthetic",
    }
    manifest_path = directory / "synthetic.json"
    manifest_path.write_text(json.dumps(manifest))
    return manifest_path


def run_validator(manifest: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["python3", str(VALIDATOR), str(manifest), "--bundle-sha256",
         BUNDLE_SHA, "--bundle-bytes", "42"],
        text=True, capture_output=True, check=False,
    )


def update_integrity(manifest: Path) -> None:
    data = json.loads(manifest.read_text())
    payload = (manifest.parent / data["log_path"]).read_bytes()
    data["bytes"] = len(payload)
    data["sha256"] = hashlib.sha256(payload).hexdigest()
    manifest.write_text(json.dumps(data))


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        manifest = make_evidence(directory)
        valid = run_validator(manifest)
        assert valid.returncode == 0, valid.stderr
        summary = json.loads(valid.stdout)
        assert summary["frames"] == 60_000
        assert summary["reclaimed"] == 4

        data = json.loads(manifest.read_text())
        log_path = directory / data["log_path"]
        log_path.write_bytes(log_path.read_bytes().replace(
            b"texture_descriptor_dwords=3936",
            b"texture_descriptor_dwords=3935", 1))
        update_integrity(manifest)
        semantic_failure = run_validator(manifest)
        assert semantic_failure.returncode != 0
        assert "per-frame resource" in semantic_failure.stderr

        manifest = make_evidence(directory)
        data = json.loads(manifest.read_text())
        with (directory / data["log_path"]).open("ab") as handle:
            handle.write(b"tamper")
        integrity_failure = run_validator(manifest)
        assert integrity_failure.returncode != 0
        assert "size/hash" in integrity_failure.stderr
    print("bsp resource evidence validator tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
