#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_texture_path_lightmap_evidence.py"
BUNDLE_SHA = "2" * 64


def make_evidence(directory: Path) -> Path:
    common_frame = (
        "constant_dwords=32 texture_descriptor_dwords=3936 "
        "overlay_vertices=4 overlay_source=constant-vsharp "
        "overlay_indices=6 acquire_engine=1 gcr=00009000 "
        "poll_cycles=190 tables_gpu_span=true"
    )
    messages = [
        "LOG_BOOT_MONOTONIC_NS=0x1234",
        f"BSP_TEXTURE_PATH_BOOT schema=1 slice=dynamic-lightmap target=gfx1013 fw=test transient_slots=2 ownership=fence+videoout bundle_sha256={BUNDLE_SHA} bundle_bytes=42 soak_frames=10000",
        "BSP_NOCLIP_PAD_READY sticks=dual triggers=vertical connected_required=true",
        "BSP_BUNDLE_READY vertices=16893 indices=29013 map_draws=3611 scene_draws=3612 draw_dwords=1 command_slot_bytes=1 allocation_bytes=1",
        "RESOURCE_HEAP_READY bytes=34000000 allocations=6 pool=fence-retired transient_slots=2 slot_bytes=131072",
        "DYNAMIC_LIGHTMAP_READY image=512x760 image_bytes=1556480 patch=497,689+8x8 hit_face=3100 patch_bytes=256 dirty_span_bytes=14368 slots=2 guards=256 selection=center-ray",
        "BSP_TEXTURE_TABLE_LAYOUT textures=164 descriptor_dwords=3936 storage=per-frame-transient lightmap=512x760 base_mode=repeat lightmap_mode=clamp filter=bilinear composition=base_x_lightmap",
        "RESOURCE_PIPELINES_READY count=2 map=bsp_resource overlay=bsp_overlay map_gs_words=2 overlay_gs_words=1 overlay_depth=disabled",
        "BSP_LOOP_BEGIN mode=texture-path-lightmap-soak buffers=2 color_dma=false depth_dma=true indexed=true frames=10000 dynamic_lightmap=bounded descriptors=per-frame overlay=fixed retirement=fence+videoout",
        "DYNAMIC_LIGHTMAP_FRAME frame=0 slot=0 pattern=0 patch_hash=1111111111111111 first_upload=true patch_bytes=256 lightmap_upload_bytes=1556480 dirty_span_bytes=1556480 acquire_bytes=1556480 resident_bytes=33000000 uploaded_bytes=1652480 transient_upload_bytes=96000 total_uploaded_bytes=1652480",
        f"RESOURCE_FRAME_READY frame=0 slot=0 transient_bytes=96000 {common_frame}",
        "RESOURCE_FRAME_SEALED frame=0 slot=0 token=100 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=0 slot=0 token=100 command_dwords=163",
        "RESOURCE_FRAME_RETIRED frame=0 slot=0 token=100 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=0 buffer=0 expected=100 observed=100 exact=true",
        "DYNAMIC_LIGHTMAP_FRAME frame=1 slot=1 pattern=1 patch_hash=2222222222222222 first_upload=true patch_bytes=256 lightmap_upload_bytes=1556480 dirty_span_bytes=1556480 acquire_bytes=1556480 resident_bytes=33000000 uploaded_bytes=1652480 transient_upload_bytes=96000 total_uploaded_bytes=3304960",
        "DYNAMIC_LIGHTMAP_FRAME frame=9998 slot=0 pattern=0 patch_hash=1111111111111111 first_upload=false patch_bytes=256 lightmap_upload_bytes=256 dirty_span_bytes=14368 acquire_bytes=14592 resident_bytes=33000000 uploaded_bytes=96256 transient_upload_bytes=96000 total_uploaded_bytes=900000000",
        "DYNAMIC_LIGHTMAP_FRAME frame=9999 slot=1 pattern=1 patch_hash=2222222222222222 first_upload=false patch_bytes=256 lightmap_upload_bytes=256 dirty_span_bytes=14368 acquire_bytes=14592 resident_bytes=33000000 uploaded_bytes=96256 transient_upload_bytes=96000 total_uploaded_bytes=900096256",
        f"RESOURCE_FRAME_READY frame=9999 slot=1 transient_bytes=96000 {common_frame}",
        "RESOURCE_FRAME_SEALED frame=9999 slot=1 token=10099 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=9999 slot=1 token=10099 command_dwords=163",
        "RESOURCE_FRAME_RETIRED frame=9999 slot=1 token=10099 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=9999 buffer=1 expected=10099 observed=10099 exact=true",
        "BSP_FRAME frame=9999 completed=10000 compose_avg_ns=1 gpu_wait_avg_ns=1 video_wait_avg_ns=1 present_interval_avg_ns=1 present_interval_max_ns=1 present_interval_over_budget=0 errors=0 terminal=0",
        "RESOURCE_RING_RETIRED slots=2 reusable=true tokens=exact last_token=10099",
        "BSP_RESOURCE_READBACK buffer0=3333333333333333 buffer1=5555555555555555 bytes=64 bright_pixels0=1 bright_pixels1=1 guards=intact frames=10000 errors=0 overlay=transient",
        "DYNAMIC_LIGHTMAP_READBACK pattern0=1111111111111111 pattern1=2222222222222222 gpu_buffer0=3333333333333333 gpu_buffer1=5555555555555555 buffers_distinct=true surrounding=stable guards=intact frames=10000",
        "BSP_TEXTURE_PATH_LIGHTMAP_COMPLETE frames=10000 resident_bytes=33000000 uploaded_bytes=900096256 patch=497,689+8x8 hit_face=3100 patterns=alternating readback=gpu-visible surrounding=stable tokens=exact guards=intact errors=0",
        "BSP_RESOURCE_SOAK_COMPLETE frames=10000 connected_frames=10000 read_errors=0 textures=164 descriptor_dwords=3936 constants=per-frame overlay=transient pipelines=2 tokens=exact guards=intact errors=0",
        "RESOURCE_POOL_RETIRED token=10099 reclaimed=6 completion=fence+videoout",
    ]
    records = [f"{index}\t{index}\tMARK\t{message}"
               for index, message in enumerate(messages, 1)]
    reason = "bsp-texture-path-lightmap-soak-complete"
    transcript = "\n".join(
        ["HELLO ps5log/1 title=PPSA99997 app=ps5-agc-gears boot=0x1234 tag=test"]
        + records + [f"BYE seq={len(records)} reason={reason}", ""]
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
        "bye_fields": {"seq": str(len(records)), "reason": reason},
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
        assert summary["frames"] == 10_000
        assert summary["reclaimed"] == 6
        assert summary["pattern0"] != summary["pattern1"]

        data = json.loads(manifest.read_text())
        log_path = directory / data["log_path"]
        log_path.write_bytes(log_path.read_bytes().replace(
            b"buffers_distinct=true", b"buffers_distinct=false", 1))
        update_integrity(manifest)
        semantic_failure = run_validator(manifest)
        assert semantic_failure.returncode != 0
        assert "GPU-visible" in semantic_failure.stderr

        manifest = make_evidence(directory)
        data = json.loads(manifest.read_text())
        with (directory / data["log_path"]).open("ab") as handle:
            handle.write(b"tamper")
        integrity_failure = run_validator(manifest)
        assert integrity_failure.returncode != 0
        assert "size/hash" in integrity_failure.stderr
    print("texture-path lightmap evidence validator tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
