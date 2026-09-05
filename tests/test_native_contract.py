#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    source = (ROOT / "native/main.c").read_text(encoding="utf-8")
    builder = (ROOT / "tools/build_native.sh").read_text(encoding="utf-8")
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    assets = (ROOT / "native/shader_assets.S").read_text(encoding="utf-8")
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
    print("native telemetry and teardown source contract passed")


if __name__ == "__main__":
    main()
