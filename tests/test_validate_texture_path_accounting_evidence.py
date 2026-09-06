#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_texture_path_accounting_evidence.py"
BUNDLE_SHA = "7" * 64


def make_evidence(directory: Path) -> Path:
    transient = 1000
    image = 10000
    patch = 256
    transient_total = transient * 10000
    lightmap_total = image * 2 + patch * 9998
    upload_total = transient_total + lightmap_total
    common = (
        "constant_dwords=32 texture_descriptor_dwords=240 "
        "overlay_vertices=4 overlay_source=constant-vsharp overlay_indices=6 "
        "acquire_engine=1 gcr=00009000 poll_cycles=190 tables_gpu_span=true"
    )
    messages = [
        "LOG_BOOT_MONOTONIC_NS=0x1234",
        f"BSP_TEXTURE_PATH_BOOT schema=1 slice=accounting target=gfx1013 fw=test transient_slots=2 ownership=fence+videoout bundle_sha256={BUNDLE_SHA} bundle_bytes=42 soak_frames=10000 input_gate=not-repeated",
        "BSP_NOCLIP_PAD_READY sticks=dual triggers=vertical connected_required=false",
        "RESOURCE_HEAP_READY bytes=200000 allocations=6 pool=fence-retired transient_slots=2 slot_bytes=15000",
        "DYNAMIC_LIGHTMAP_READY image=50x50 image_bytes=10000 patch=1,1+8x8 hit_face=3 patch_bytes=256 dirty_span_bytes=512 slots=2 guards=256 selection=center-ray",
        "TEXTURE_RESIDENCY_READY schema=1 pool_capacity_bytes=200000 pool_resident_bytes=141024 bsp_allocation_bytes=60000 shader_allocation_bytes=10000 depth_allocation_bytes=20000 transient_allocation_bytes=30000 lightmap_allocation_bytes=21024 texture_payload_bytes=60000 base_texture_bytes=30000 source_lightmap_bytes=10000 dynamic_lightmap_image_bytes=20000 lightmap_slots=2 allocations=6 accounting=exact",
        "MIP_CHAINS_READY textures=10 layout=addr-sw-linear order=smallest-to-base pitch_alignment=256 levels_min=5 levels_max=9 chain_bytes=30000 deterministic=box-rne",
        "BSP_TEXTURE_TABLE_LAYOUT textures=10 descriptor_dwords=240 storage=per-frame-transient lightmap=50x50 base_mode=repeat lightmap_mode=clamp base_filter=anisotropic4x lightmap_filter=bilinear composition=base_x_lightmap",
        "RESOURCE_PIPELINES_READY count=4 map=bsp_resource alpha_test=bsp_alpha_test sky=bsp_sky overlay=bsp_overlay map_gs_words=2 alpha_gs_words=2 sky_gs_words=2 overlay_gs_words=1 overlay_depth=disabled",
        "TEXTURE_FEATURES_READY mip_chains=10 opaque_draws=80 alpha_draws=10 sky_draws=10 total_draws=100 pipelines=4 composition=opaque+alpha+sky sampler=anisotropic4x",
        "BSP_LOOP_BEGIN mode=texture-path-accounting-soak buffers=2 color_dma=false depth_dma=true indexed=true frames=10000 opaque_pass=separate alpha_test_pass=separate sky_pass=separate sampler=anisotropic4x dynamic_lightmap=bounded residency=partitioned uploads=checked-per-frame descriptors=per-frame overlay=fixed retirement=fence+videoout input_dependency=none",
        "DYNAMIC_LIGHTMAP_FRAME frame=0 slot=0 pattern=0 patch_hash=1111111111111111 first_upload=true patch_bytes=256 lightmap_upload_bytes=10000 dirty_span_bytes=10000 acquire_bytes=10000 resident_bytes=141024 uploaded_bytes=11000 transient_upload_bytes=1000 total_uploaded_bytes=11000",
        "TEXTURE_UPLOAD_FRAME schema=1 frame=0 slot=0 first_upload=true transient_bytes=1000 lightmap_bytes=10000 frame_bytes=11000 cumulative_transient_bytes=1000 cumulative_lightmap_bytes=10000 cumulative_bytes=11000 sequence_hash=1111111111111111 accounting=checked-u64",
        f"RESOURCE_FRAME_READY frame=0 slot=0 transient_bytes=1000 {common}",
        "RESOURCE_FRAME_SEALED frame=0 slot=0 token=100 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=0 slot=0 token=100 command_dwords=30000",
        "RESOURCE_FRAME_RETIRED frame=0 slot=0 token=100 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=0 buffer=0 expected=100 observed=100 exact=true",
        "DYNAMIC_LIGHTMAP_FRAME frame=1 slot=1 pattern=1 patch_hash=2222222222222222 first_upload=true patch_bytes=256 lightmap_upload_bytes=10000 dirty_span_bytes=10000 acquire_bytes=10000 resident_bytes=141024 uploaded_bytes=11000 transient_upload_bytes=1000 total_uploaded_bytes=22000",
        "TEXTURE_UPLOAD_FRAME schema=1 frame=1 slot=1 first_upload=true transient_bytes=1000 lightmap_bytes=10000 frame_bytes=11000 cumulative_transient_bytes=2000 cumulative_lightmap_bytes=20000 cumulative_bytes=22000 sequence_hash=2222222222222222 accounting=checked-u64",
        "DYNAMIC_LIGHTMAP_FRAME frame=9998 slot=0 pattern=0 patch_hash=1111111111111111 first_upload=false patch_bytes=256 lightmap_upload_bytes=256 dirty_span_bytes=512 acquire_bytes=768 resident_bytes=141024 uploaded_bytes=1256 transient_upload_bytes=1000 total_uploaded_bytes=12578232",
        "TEXTURE_UPLOAD_FRAME schema=1 frame=9998 slot=0 first_upload=false transient_bytes=1000 lightmap_bytes=256 frame_bytes=1256 cumulative_transient_bytes=9999000 cumulative_lightmap_bytes=2579232 cumulative_bytes=12578232 sequence_hash=3333333333333333 accounting=checked-u64",
        f"DYNAMIC_LIGHTMAP_FRAME frame=9999 slot=1 pattern=1 patch_hash=2222222222222222 first_upload=false patch_bytes=256 lightmap_upload_bytes=256 dirty_span_bytes=512 acquire_bytes=768 resident_bytes=141024 uploaded_bytes=1256 transient_upload_bytes=1000 total_uploaded_bytes={upload_total}",
        f"TEXTURE_UPLOAD_FRAME schema=1 frame=9999 slot=1 first_upload=false transient_bytes=1000 lightmap_bytes=256 frame_bytes=1256 cumulative_transient_bytes=10000000 cumulative_lightmap_bytes={lightmap_total} cumulative_bytes={upload_total} sequence_hash=4444444444444444 accounting=checked-u64",
        f"RESOURCE_FRAME_READY frame=9999 slot=1 transient_bytes=1000 {common}",
        "RESOURCE_FRAME_SEALED frame=9999 slot=1 token=10099 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=9999 slot=1 token=10099 command_dwords=30000",
        "RESOURCE_FRAME_RETIRED frame=9999 slot=1 token=10099 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=9999 buffer=1 expected=10099 observed=10099 exact=true",
        "BSP_FRAME frame=9999 completed=10000 compose_avg_ns=1 gpu_wait_avg_ns=1 video_wait_avg_ns=1 present_interval_avg_ns=1 present_interval_max_ns=1 present_interval_over_budget=0 errors=0 terminal=0",
        "RESOURCE_RING_RETIRED slots=2 reusable=true tokens=exact last_token=10099",
        "BSP_RESOURCE_READBACK buffer0=3333333333333333 buffer1=7777777777777777 bytes=64 bright_pixels0=1 bright_pixels1=1 guards=intact frames=10000 errors=0 overlay=transient",
        "DYNAMIC_LIGHTMAP_READBACK pattern0=1111111111111111 pattern1=2222222222222222 gpu_buffer0=3333333333333333 gpu_buffer1=7777777777777777 buffers_distinct=true surrounding=stable guards=intact frames=10000",
        f"TEXTURE_UPLOAD_SUMMARY schema=1 frames=10000 transient_bytes_per_frame=1000 bounded_lightmap_bytes_per_frame=256 transient_bytes_total=10000000 lightmap_bytes_total={lightmap_total} upload_bytes_total={upload_total} frame_bytes_min=1256 frame_bytes_max=11000 full_upload_frames=2 bounded_upload_frames=9998 sequence_hash=4444444444444444 sequence=gap-free accounting=checked-u64",
        f"BSP_TEXTURE_PATH_LIGHTMAP_COMPLETE frames=10000 resident_bytes=141024 uploaded_bytes={upload_total} patch=1,1+8x8 hit_face=3 patterns=alternating readback=gpu-visible surrounding=stable tokens=exact guards=intact errors=0",
        f"BSP_TEXTURE_PATH_ACCOUNTING_COMPLETE frames=10000 pool_resident_bytes=141024 texture_payload_bytes=60000 upload_bytes_total={upload_total} sequence_hash=4444444444444444 allocations=6 accounting=exact+gap-free tokens=exact guards=intact input_dependency=none errors=0",
        "BSP_RESOURCE_SOAK_COMPLETE frames=10000 connected_frames=0 read_errors=0 textures=10 descriptor_dwords=240 constants=per-frame overlay=transient pipelines=4 input_dependency=none tokens=exact guards=intact errors=0",
        "RESOURCE_POOL_RETIRED token=10099 reclaimed=6 completion=fence+videoout",
    ]
    records = [f"{index}\t{index}\tMARK\t{message}"
               for index, message in enumerate(messages, 1)]
    reason = "bsp-texture-path-accounting-soak-complete"
    payload = "\n".join(
        ["HELLO ps5log/1 title=PPSA99997 app=ps5-agc-gears boot=0x1234 tag=test"]
        + records + [f"BYE seq={len(records)} reason={reason}", ""]
    ).encode()
    (directory / "synthetic.log").write_bytes(payload)
    manifest = {
        "identity": {"title": "PPSA99997", "app": "ps5-agc-gears",
                     "boot": "0x1234"},
        "protocol": "ps5log/1", "transport": "tcp", "hello": True,
        "bye": True, "clean": True, "gaps": [], "raw_lines": 0,
        "oversized_lines": 0, "records": len(records),
        "last_seq": len(records),
        "bye_fields": {"seq": str(len(records)), "reason": reason},
        "log_path": "synthetic.log", "bytes": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(), "run_id": "synthetic",
    }
    path = directory / "synthetic.json"
    path.write_text(json.dumps(manifest))
    return path


def run_validator(path: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["python3", str(VALIDATOR), str(path), "--bundle-sha256", BUNDLE_SHA,
         "--bundle-bytes", "42"], text=True, capture_output=True, check=False
    )


def refresh(path: Path) -> None:
    manifest = json.loads(path.read_text())
    data = (path.parent / manifest["log_path"]).read_bytes()
    manifest["bytes"] = len(data)
    manifest["sha256"] = hashlib.sha256(data).hexdigest()
    path.write_text(json.dumps(manifest))


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        path = make_evidence(directory)
        valid = run_validator(path)
        assert valid.returncode == 0, valid.stderr
        summary = json.loads(valid.stdout)
        assert summary["frames"] == 10_000
        assert summary["upload_bytes_total"] == 12_579_488

        log = directory / "synthetic.log"
        log.write_bytes(log.read_bytes().replace(
            b"bounded_upload_frames=9998", b"bounded_upload_frames=9997"
        ))
        refresh(path)
        semantic = run_validator(path)
        assert semantic.returncode != 0 and "summary" in semantic.stderr

        path = make_evidence(directory)
        log = directory / "synthetic.log"
        log.write_bytes(log.read_bytes().replace(
            b"BSP_RESOURCE_READBACK buffer0=3333333333333333",
            b"BSP_RESOURCE_READBACK buffer0=9999999999999999",
        ))
        refresh(path)
        disconnected = run_validator(path)
        assert disconnected.returncode != 0 and \
            "terminal buffers" in disconnected.stderr

        path = make_evidence(directory)
        with (directory / "synthetic.log").open("ab") as stream:
            stream.write(b"tamper")
        integrity = run_validator(path)
        assert integrity.returncode != 0 and "size/hash" in integrity.stderr
    print("texture-path accounting evidence validator tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
