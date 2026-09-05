#!/usr/bin/env python3
"""Check the independently authored LLPC pipe contract without compiling it."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    text = (ROOT / "shaders/gears_lit.pipe").read_text(encoding="utf-8")
    required = (
        "version = 65",
        "layout(location = 0) in vec3 in_position;",
        "layout(location = 1) in vec3 in_normal;",
        "mat4 mvp;",
        "vec4 rotation;",
        "vec4 material;",
        "userDataNode[0].sizeInDwords = 24",
        "userDataNode[1].offsetInDwords = 24",
        "userDataNode[1].type = IndirectUserDataVaPtr",
        "topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST",
        "colorBuffer[0].format = VK_FORMAT_B8G8R8A8_UNORM",
        "binding[0].stride = 24",
        "attribute[1].offset = 12",
    )
    for value in required:
        if text.count(value) != 1:
            raise SystemExit(f"shader contract is missing or duplicates: {value}")
    forbidden = ("gfx" + "1030", ".text.bin", ".pal.elf", "/home/", "/data/")
    for value in forbidden:
        if value in text:
            raise SystemExit(f"shader source contains forbidden value: {value}")
    flat = (ROOT / "shaders/bsp_flat.pipe").read_text(encoding="utf-8")
    flat_required = (
        "layout(location = 0) in vec3 in_position;",
        "mat4 mvp;",
        "vec4 face_color;",
        "userDataNode[0].sizeInDwords = 20",
        "userDataNode[1].offsetInDwords = 20",
        "binding[0].stride = 32",
        "topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST",
    )
    for value in flat_required:
        if flat.count(value) != 1:
            raise SystemExit(f"BSP flat shader contract is missing: {value}")
    for value in forbidden:
        if value in flat:
            raise SystemExit(f"BSP flat shader contains forbidden value: {value}")
    print("shader source contract passed: gears 24+1 and BSP flat 20+1 DWORD ABIs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
