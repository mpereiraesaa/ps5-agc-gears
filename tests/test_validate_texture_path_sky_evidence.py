#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import json
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "tools" / "validate_texture_path_sky_evidence.py"
BUNDLE_SHA = "6" * 64


def make_evidence(directory: Path) -> Path:
    common = (
        "constant_dwords=32 texture_descriptor_dwords=2928 "
        "overlay_vertices=4 overlay_source=constant-vsharp overlay_indices=6 "
        "acquire_engine=1 gcr=00009000 poll_cycles=190 tables_gpu_span=true"
    )
    dynamic = (
        "first_upload=false patch_bytes=256 lightmap_upload_bytes=256 "
        "dirty_span_bytes=28704 acquire_bytes=28928 resident_bytes=70000000 "
        "uploaded_bytes=17052 transient_upload_bytes=16796"
    )
    messages = [
        "LOG_BOOT_MONOTONIC_NS=0x1234",
        f"BSP_TEXTURE_PATH_BOOT schema=1 slice=sky target=gfx1013 fw=test transient_slots=2 ownership=fence+videoout bundle_sha256={BUNDLE_SHA} bundle_bytes=42 soak_frames=10000",
        "MIP_CHAINS_READY textures=122 layout=addr-sw-linear order=smallest-to-base pitch_alignment=256 levels_min=5 levels_max=9 chain_bytes=6082816 deterministic=box-rne",
        "BSP_TEXTURE_TABLE_LAYOUT textures=122 descriptor_dwords=2928 storage=per-frame-transient lightmap=1024x502 base_mode=repeat lightmap_mode=clamp base_filter=anisotropic4x lightmap_filter=bilinear composition=base_x_lightmap",
        "RESOURCE_PIPELINES_READY count=4 map=bsp_resource alpha_test=bsp_alpha_test sky=bsp_sky overlay=bsp_overlay map_gs_words=2 alpha_gs_words=2 sky_gs_words=2 overlay_gs_words=1 overlay_depth=disabled",
        "SKY_PASS_READY textures=1 draws=158 opaque_draws=2915 alpha_draws=137 target_texture=53 target_face=1318 camera=nearest-centroid-standoff pipeline=bsp_sky composition=base-unlit depth_write=enabled blend=disabled opaque_ps_bytes=260 sky_ps_bytes=204 shader_distinct=true",
        "BSP_LOOP_BEGIN mode=texture-path-sky-soak buffers=2 color_dma=false depth_dma=true indexed=true frames=10000 opaque_pass=separate alpha_test_pass=separate sky_pass=separate sampler=anisotropic4x lightmap_pattern=paired descriptors=per-frame overlay=fixed retirement=fence+videoout",
        "DYNAMIC_LIGHTMAP_FRAME frame=0 slot=0 pattern=0 patch_hash=1111111111111111 first_upload=true patch_bytes=256 lightmap_upload_bytes=2056192 dirty_span_bytes=2056192 acquire_bytes=2056192 resident_bytes=70000000 uploaded_bytes=2072988 transient_upload_bytes=16796 total_uploaded_bytes=2072988",
        "SKY_PASS_FRAME frame=0 slot=0 mode=sky-pass sky_draws=158 expected_sky_draws=158 sampler=anisotropic4x lightmap_pattern=0",
        f"RESOURCE_FRAME_READY frame=0 slot=0 transient_bytes=16796 {common}",
        "RESOURCE_FRAME_SEALED frame=0 slot=0 token=100 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=0 slot=0 token=100 command_dwords=30000",
        "RESOURCE_FRAME_RETIRED frame=0 slot=0 token=100 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=0 buffer=0 expected=100 observed=100 exact=true",
        "DYNAMIC_LIGHTMAP_FRAME frame=1 slot=1 pattern=0 patch_hash=1111111111111111 first_upload=true patch_bytes=256 lightmap_upload_bytes=2056192 dirty_span_bytes=2056192 acquire_bytes=2056192 resident_bytes=70000000 uploaded_bytes=2072988 transient_upload_bytes=16796 total_uploaded_bytes=4145976",
        f"DYNAMIC_LIGHTMAP_FRAME frame=9998 slot=0 pattern=1 patch_hash=2222222222222222 {dynamic} total_uploaded_bytes=170000000",
        "SKY_PASS_FRAME frame=9998 slot=0 mode=skip-control sky_draws=0 expected_sky_draws=158 sampler=anisotropic4x lightmap_pattern=1",
        f"DYNAMIC_LIGHTMAP_FRAME frame=9999 slot=1 pattern=1 patch_hash=2222222222222222 {dynamic} total_uploaded_bytes=170017052",
        "SKY_PASS_FRAME frame=9999 slot=1 mode=sky-pass sky_draws=158 expected_sky_draws=158 sampler=anisotropic4x lightmap_pattern=1",
        f"RESOURCE_FRAME_READY frame=9999 slot=1 transient_bytes=16796 {common}",
        "RESOURCE_FRAME_SEALED frame=9999 slot=1 token=10099 retirement=fence+videoout",
        "RESOURCE_FRAME_SUBMITTED frame=9999 slot=1 token=10099 command_dwords=30000",
        "RESOURCE_FRAME_RETIRED frame=9999 slot=1 token=10099 fence=zero videoout_token=exact",
        "BSP_VIDEOOUT_TOKEN frame=9999 buffer=1 expected=10099 observed=10099 exact=true",
        "BSP_FRAME frame=9999 completed=10000 compose_avg_ns=1 gpu_wait_avg_ns=1 video_wait_avg_ns=1 present_interval_avg_ns=1 present_interval_max_ns=1 present_interval_over_budget=0 errors=0 terminal=0",
        "RESOURCE_RING_RETIRED slots=2 reusable=true tokens=exact last_token=10099",
        "BSP_RESOURCE_READBACK buffer0=3333333333333333 buffer1=7777777777777777 bytes=64 bright_pixels0=1 bright_pixels1=1 guards=intact frames=10000 errors=0 overlay=transient",
        "DYNAMIC_LIGHTMAP_READBACK slot0=2222222222222222 slot1=2222222222222222 final_pattern=1 slots_equal=true surrounding=stable guards=intact frames=10000",
        "SKY_PASS_READBACK skip_control_buffer=3333333333333333 sky_pass_buffer=7777777777777777 bytes=64 sampler=anisotropic4x lightmap_pattern=1 paired=true framebuffer_distinct=true guards=intact frames=10000",
        "BSP_TEXTURE_PATH_SKY_COMPLETE frames=10000 textures=1 draws=158 pipeline=separate skip_control=gpu-visible sky_pass=gpu-visible paired_lightmap=true tokens=exact guards=intact errors=0",
        "BSP_RESOURCE_SOAK_COMPLETE frames=10000 connected_frames=10000 read_errors=0 textures=122 descriptor_dwords=2928 constants=per-frame overlay=transient pipelines=4 tokens=exact guards=intact errors=0",
        "RESOURCE_POOL_RETIRED token=10099 reclaimed=6 completion=fence+videoout",
    ]
    records = [f"{index}\t{index}\tMARK\t{message}"
               for index, message in enumerate(messages, 1)]
    reason = "bsp-texture-path-sky-soak-complete"
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
        "oversized_lines": 0, "records": len(records), "last_seq": len(records),
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
        assert summary["frames"] == 10_000 and summary["draws"] == 158

        log = directory / "synthetic.log"
        log.write_bytes(log.read_bytes().replace(
            b"mode=skip-control sky_draws=0",
            b"mode=skip-control sky_draws=1",
        ))
        refresh(path)
        semantic = run_validator(path)
        assert semantic.returncode != 0 and "isolation" in semantic.stderr

        path = make_evidence(directory)
        with (directory / "synthetic.log").open("ab") as stream:
            stream.write(b"tamper")
        integrity = run_validator(path)
        assert integrity.returncode != 0 and "size/hash" in integrity.stderr
    print("texture-path sky evidence validator tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
