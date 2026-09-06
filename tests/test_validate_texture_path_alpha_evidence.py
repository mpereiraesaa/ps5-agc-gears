#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_texture_path_alpha_evidence.py"
BUNDLE_SHA = "4" * 64


def make_evidence(directory: Path) -> Path:
    common = (
        "constant_dwords=32 texture_descriptor_dwords=3936 "
        "overlay_vertices=4 overlay_source=constant-vsharp overlay_indices=6 "
        "acquire_engine=1 gcr=00009000 poll_cycles=190 tables_gpu_span=true"
    )
    dynamic = (
        "first_upload=false patch_bytes=256 lightmap_upload_bytes=256 "
        "dirty_span_bytes=14368 acquire_bytes=14592 resident_bytes=33000000 "
        "uploaded_bytes=96256 transient_upload_bytes=96000"
    )
    messages = [
        "LOG_BOOT_MONOTONIC_NS=0x1234",
        f"BSP_TEXTURE_PATH_BOOT schema=1 slice=alpha-test target=gfx1013 fw=test transient_slots=2 ownership=fence+videoout bundle_sha256={BUNDLE_SHA} bundle_bytes=42 soak_frames=10000",
        "MIP_CHAINS_READY textures=164 layout=addr-sw-linear order=smallest-to-base pitch_alignment=256 levels_min=5 levels_max=9 chain_bytes=7000000 deterministic=box-rne",
        "BSP_TEXTURE_TABLE_LAYOUT textures=164 descriptor_dwords=3936 storage=per-frame-transient lightmap=512x760 base_mode=repeat lightmap_mode=clamp base_filter=anisotropic4x lightmap_filter=bilinear composition=base_x_lightmap",
        "RESOURCE_PIPELINES_READY count=3 map=bsp_resource alpha_test=bsp_alpha_test overlay=bsp_overlay map_gs_words=2 alpha_gs_words=2 overlay_gs_words=1 overlay_depth=disabled",
        "ALPHA_TEST_READY textures=1 draws=42 opaque_draws=3569 target_texture=125 target_face=3245 camera=nearest-centroid opaque_pipeline=bsp_resource alpha_pipeline=bsp_alpha_test opaque_db=00000810 alpha_db=00000850 kill_bit=0x40 threshold=0.5 depth_write=enabled blend=disabled",
        "BSP_LOOP_BEGIN mode=texture-path-alpha-soak buffers=2 color_dma=false depth_dma=true indexed=true frames=10000 opaque_pass=separate alpha_test_pass=separate sampler=anisotropic4x lightmap_pattern=paired descriptors=per-frame overlay=fixed retirement=fence+videoout",
        "DYNAMIC_LIGHTMAP_FRAME frame=0 slot=0 pattern=0 patch_hash=1111111111111111 first_upload=true patch_bytes=256 lightmap_upload_bytes=1556480 dirty_span_bytes=1556480 acquire_bytes=1556480 resident_bytes=33000000 uploaded_bytes=1652480 transient_upload_bytes=96000 total_uploaded_bytes=1652480",
        "ALPHA_TEST_FRAME frame=0 slot=0 mode=alpha-test opaque_draws=3569 alpha_test_draws=42 sampler=anisotropic4x lightmap_pattern=0",
        f"RESOURCE_FRAME_READY frame=0 slot=0 transient_bytes=96000 {common}",
        "RESOURCE_FRAME_SEALED frame=0 slot=0 token=100 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=0 slot=0 token=100 command_dwords=163",
        "RESOURCE_FRAME_RETIRED frame=0 slot=0 token=100 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=0 buffer=0 expected=100 observed=100 exact=true",
        "DYNAMIC_LIGHTMAP_FRAME frame=1 slot=1 pattern=0 patch_hash=1111111111111111 first_upload=true patch_bytes=256 lightmap_upload_bytes=1556480 dirty_span_bytes=1556480 acquire_bytes=1556480 resident_bytes=33000000 uploaded_bytes=1652480 transient_upload_bytes=96000 total_uploaded_bytes=3304960",
        f"DYNAMIC_LIGHTMAP_FRAME frame=9998 slot=0 pattern=1 patch_hash=2222222222222222 {dynamic} total_uploaded_bytes=900000000",
        "ALPHA_TEST_FRAME frame=9998 slot=0 mode=opaque-control opaque_draws=3569 alpha_test_draws=42 sampler=anisotropic4x lightmap_pattern=1",
        f"DYNAMIC_LIGHTMAP_FRAME frame=9999 slot=1 pattern=1 patch_hash=2222222222222222 {dynamic} total_uploaded_bytes=900096256",
        "ALPHA_TEST_FRAME frame=9999 slot=1 mode=alpha-test opaque_draws=3569 alpha_test_draws=42 sampler=anisotropic4x lightmap_pattern=1",
        f"RESOURCE_FRAME_READY frame=9999 slot=1 transient_bytes=96000 {common}",
        "RESOURCE_FRAME_SEALED frame=9999 slot=1 token=10099 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=9999 slot=1 token=10099 command_dwords=163",
        "RESOURCE_FRAME_RETIRED frame=9999 slot=1 token=10099 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=9999 buffer=1 expected=10099 observed=10099 exact=true",
        "BSP_FRAME frame=9999 completed=10000 compose_avg_ns=1 gpu_wait_avg_ns=1 video_wait_avg_ns=1 present_interval_avg_ns=1 present_interval_max_ns=1 present_interval_over_budget=0 errors=0 terminal=0",
        "RESOURCE_RING_RETIRED slots=2 reusable=true tokens=exact last_token=10099",
        "BSP_RESOURCE_READBACK buffer0=3333333333333333 buffer1=5555555555555555 bytes=64 bright_pixels0=1 bright_pixels1=1 guards=intact frames=10000 errors=0 overlay=transient",
        "DYNAMIC_LIGHTMAP_READBACK slot0=2222222222222222 slot1=2222222222222222 final_pattern=1 slots_equal=true surrounding=stable guards=intact frames=10000",
        "ALPHA_TEST_READBACK opaque_control_buffer=3333333333333333 alpha_test_buffer=5555555555555555 bytes=64 sampler=anisotropic4x lightmap_pattern=1 paired=true framebuffer_distinct=true guards=intact frames=10000",
        "BSP_TEXTURE_PATH_ALPHA_COMPLETE frames=10000 textures=1 draws=42 pipeline=separate opaque_control=gpu-visible alpha_test=gpu-visible paired_lightmap=true tokens=exact guards=intact errors=0",
        "BSP_RESOURCE_SOAK_COMPLETE frames=10000 connected_frames=10000 read_errors=0 textures=164 descriptor_dwords=3936 constants=per-frame overlay=transient pipelines=3 tokens=exact guards=intact errors=0",
        "RESOURCE_POOL_RETIRED token=10099 reclaimed=6 completion=fence+videoout",
    ]
    records = [f"{index}\t{index}\tMARK\t{message}"
               for index, message in enumerate(messages, 1)]
    reason = "bsp-texture-path-alpha-soak-complete"
    transcript = "\n".join(
        ["HELLO ps5log/1 title=PPSA99997 app=ps5-agc-gears boot=0x1234 tag=test"]
        + records + [f"BYE seq={len(records)} reason={reason}", ""]
    ).encode()
    (directory / "synthetic.log").write_bytes(transcript)
    manifest = {
        "identity": {"title": "PPSA99997", "app": "ps5-agc-gears",
                     "boot": "0x1234"},
        "protocol": "ps5log/1", "transport": "tcp", "hello": True,
        "bye": True, "clean": True, "gaps": [], "raw_lines": 0,
        "oversized_lines": 0, "records": len(records), "last_seq": len(records),
        "bye_fields": {"seq": str(len(records)), "reason": reason},
        "log_path": "synthetic.log", "bytes": len(transcript),
        "sha256": hashlib.sha256(transcript).hexdigest(), "run_id": "synthetic",
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
        assert summary["frames"] == 10_000 and summary["draws"] == 42

        log = directory / "synthetic.log"
        log.write_bytes(log.read_bytes().replace(
            b"framebuffer_distinct=true", b"framebuffer_distinct=false"
        ))
        refresh(path)
        semantic = run_validator(path)
        assert semantic.returncode != 0 and "GPU-visible" in semantic.stderr

        path = make_evidence(directory)
        log.write_bytes(log.read_bytes().replace(
            b"alpha_db=00000850", b"alpha_db=00000870"
        ))
        refresh(path)
        register_delta = run_validator(path)
        assert register_delta.returncode != 0 and \
            "classification/register" in register_delta.stderr

        path = make_evidence(directory)
        with (directory / "synthetic.log").open("ab") as stream:
            stream.write(b"tamper")
        integrity = run_validator(path)
        assert integrity.returncode != 0 and "size/hash" in integrity.stderr
    print("texture-path alpha evidence validator tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
