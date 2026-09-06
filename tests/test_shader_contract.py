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
    textured = (ROOT / "shaders/bsp_textured.pipe").read_text(encoding="utf-8")
    textured_required = (
        "layout(location = 1) in vec2 in_base_uv;",
        "layout(location = 2) in vec2 in_light_uv;",
        "uniform sampler2D base_texture;",
        "uniform sampler2D lightmap_texture;",
        "base.rgb * texture(lightmap_texture, light_uv).rgb",
        "userDataNode[2].type = DescriptorTableVaPtr",
        "userDataNode[2].offsetInDwords = 0",
        "userDataNode[2].next[0].type = DescriptorCombinedTexture",
        "userDataNode[2].next[0].offsetInDwords = 0",
        "userDataNode[2].next[1].offsetInDwords = 12",
        "attribute[1].offset = 12",
        "attribute[2].offset = 20",
    )
    for value in textured_required:
        if textured.count(value) != 1:
            raise SystemExit(f"BSP textured shader contract is missing: {value}")
    for value in forbidden:
        if value in textured:
            raise SystemExit(f"BSP textured shader contains forbidden value: {value}")
    resource = (ROOT / "shaders/bsp_resource.pipe").read_text(encoding="utf-8")
    resource_required = (
        "layout(set = 0, binding = 0, std140) uniform FrameConstants",
        "mat4 mvp;",
        "vec4 control;",
        "vec4 debug_values[3];",
        "userDataNode[0].sizeInDwords = 1",
        "userDataNode[0].next[0].type = DescriptorConstBuffer",
        "userDataNode[1].type = IndirectUserDataVaPtr",
        "userDataNode[2].type = DescriptorTableVaPtr",
        "binding[0].stride = 32",
    )
    for value in resource_required:
        if value not in resource:
            raise SystemExit(f"BSP resource shader contract is missing: {value}")
    if "discard;" in resource:
        raise SystemExit("opaque BSP resource permutation must not discard")
    alpha_test = (ROOT / "shaders/bsp_alpha_test.pipe").read_text(
        encoding="utf-8"
    )
    for value in resource_required + (
        "if (base.a < 0.5)",
        "discard;",
    ):
        if value not in alpha_test:
            raise SystemExit(f"BSP alpha-test shader contract is missing: {value}")
    sky = (ROOT / "shaders/bsp_sky.pipe").read_text(encoding="utf-8")
    for value in resource_required + (
        "vec4 sky = texture(base_texture, base_uv);",
        "color = vec4(sky.rgb, 1.0);",
    ):
        if value not in sky:
            raise SystemExit(f"BSP sky shader contract is missing: {value}")
    if "discard;" in sky or "base.rgb * texture(lightmap_texture" in sky:
        raise SystemExit("BSP sky permutation must be unlit and non-discarding")
    overlay = (ROOT / "shaders/bsp_overlay.pipe").read_text(encoding="utf-8")
    overlay_required = (
        "uniform OverlayConstants",
        "vec4 debug_values[7];",
        "gl_VertexIndex",
        "userDataNode[0].sizeInDwords = 1",
        "userDataNode[0].type = DescriptorTableVaPtr",
        "userDataNode[0].next[0].type = DescriptorConstBuffer",
    )
    for value in overlay_required:
        if value not in overlay:
            raise SystemExit(f"BSP overlay shader contract is missing: {value}")
    for name, pipe in (("resource", resource), ("alpha-test", alpha_test),
                       ("sky", sky), ("overlay", overlay)):
        for value in forbidden:
            if value in pipe:
                raise SystemExit(f"BSP {name} shader contains forbidden value: {value}")
    print("shader source contract passed: gears, BSP flat/textured/resource/alpha/sky/overlay ABIs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
