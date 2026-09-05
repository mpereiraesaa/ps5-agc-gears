#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    source = (ROOT / "native/main.c").read_text(encoding="utf-8")
    builder = (ROOT / "tools/build_native.sh").read_text(encoding="utf-8")
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    assets = (ROOT / "native/shader_assets.S").read_text(encoding="utf-8")
    bsp_metadata = (ROOT / "tools/generate_bsp_build_metadata.py").read_text(
        encoding="utf-8"
    )
    required = (
        '"LOG_SCHEMA=3"',
        '"LOG_TRANSPORT=ps5log/1 tcp structured"',
        '"LOG_FS_SINKS=disabled"',
        'ps5log_hex64(PS5LOG_INFO, "LOG_BOOT_MONOTONIC_NS", boot_token)',
        '"GEARS_LOOP_BEGIN mode=continuous buffers=2 "',
        'gears_frame_loop_init(&loop, &input)',
        'gears_frame_loop_step(&loop)',
        '"GEARS_HEARTBEAT frames=%llu max_in_flight=%u "',
        '"retired_fences=zero tokens=exact guards=intact errors=%llu"',
        '"present_interval_avg_ns=%llu present_interval_max_ns=%llu "',
        'input.present_interval_budget_ns = UINT64_C(17000000)',
    )
    for item in required:
        if item not in source:
            raise SystemExit(f"native telemetry/teardown contract missing: {item}")
    if 'LOG_BOOT_MONOTONIC_NS=' in source:
        raise SystemExit("ps5log_hex64 label must not contain its own equals sign")
    if 'make -C "$foundation" app' in builder:
        raise SystemExit("standalone builder must not build the foundation sample title")
    for unit in ("native_app_builder.cpp", "self_container.cpp",
                 "elf_object.cpp", "sce_module_writer.cpp"):
        if unit not in builder:
            raise SystemExit(f"foundation host tool source missing: {unit}")
    for obsolete in ("PS5_GEARS_FRAME_COUNT", "PS5_GEARS_RELEASE_CHUNK_FRAMES",
                     "PS5_GEARS_VISIBLE_HOLD_SECONDS", "gears_run_frames(",
                     "deadline_misses"):
        if obsolete in source or obsolete in builder:
            raise SystemExit(f"production runtime still contains test policy: {obsolete}")
    if 'bsp_flat_shader_metadata.h' not in makefile:
        raise SystemExit("BSP flat shader metadata build product missing")
    for item in ('bsp_flat.gs.bin', 'bsp_flat.ps.bin'):
        if item not in assets:
            raise SystemExit(f"BSP flat shader native asset missing: {item}")
    for item in ('bsp-inspect', 'generate_bsp_build_metadata.py',
                 '-DPS5_BSP_VIEWER=1', 'map.ps5bsp'):
        if item not in builder:
            raise SystemExit(f"BSP private release contract missing: {item}")
    if 'PS5_BSP_BUNDLE_SHA256' not in bsp_metadata:
        raise SystemExit("BSP bundle SHA-256 metadata contract missing")
    for item in (
        '"/app0/map.ps5bsp"', 'BSP_BUNDLE_READY vertices=%u',
        'bsp_flat_compose(', 'BSP_VIDEOOUT_TOKEN frame=%llu buffer=%u',
        'BSP_READBACK_FNV64 buffer0=%016llx',
        'BSP_GATE1_COMPLETE fixed_camera=true',
        'BSP_GATE_FRAME_COUNT = 600u', 'bright_pixel_count(',
        'geometry_visible=true tokens=exact guards=intact',
    ):
        if item not in source:
            raise SystemExit(f"BSP native gate contract missing: {item}")
    if "BSP viewer requires PS5LOG_DEV_CONF" not in builder:
        raise SystemExit("BSP release must fail closed without TCP config")
    for item in (
        "BSP_NOCLIP requires BSP_BUNDLE",
        "-DPS5_BSP_NOCLIP=1",
        "src/bsp_noclip.c",
        "src/bsp_texture_descriptor.c",
    ):
        if item not in builder:
            raise SystemExit(f"BSP noclip native build contract missing: {item}")
    for item in (
        "BSP_GATE_FRAME_COUNT = 10000u",
        "scePadReadState(",
        "bsp_noclip_step(",
        "bsp_flat_update_camera(",
        "BSP_NOCLIP_PAD_READY sticks=dual triggers=vertical",
        "BSP_LOOP_BEGIN mode=noclip-soak",
        "BSP_NOCLIP_SOAK_COMPLETE frames=%llu",
        "BSP_NOCLIP_MIN_MOVING_FRAMES = 600u",
        "BSP_NOCLIP_MIN_LOOKING_FRAMES = 120u",
        'ps5log_close("bsp-noclip-soak-complete")',
    ):
        if item not in source:
            raise SystemExit(f"BSP noclip runtime contract missing: {item}")
    if "bsp-noclip-native-release" not in makefile or "BSP_NOCLIP=1" not in makefile:
        raise SystemExit("BSP noclip release target missing")
    print("native telemetry and teardown source contract passed")


if __name__ == "__main__":
    main()
