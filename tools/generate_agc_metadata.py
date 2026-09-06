#!/usr/bin/env python3
"""Translate public LLPC PAL metadata into the demo's sanitized AGC templates."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def bit(value: object) -> int:
    return int(bool(value))


def pack(values: list[int], width: int) -> int:
    mask = (1 << width) - 1
    return sum((value & mask) << (index * width)
               for index, value in enumerate(values))


def derive(manifest: dict[str, object]) -> dict[str, object]:
    if (manifest.get("target"), manifest.get("pipeline_type"),
            manifest.get("no_relocations")) != ("gfx1013", "Ngg", True):
        raise ValueError("expected a relocation-free gfx1013 NGG pipeline")
    graphics = manifest["graphics_register_metadata"]
    stages = manifest["hardware_stages"]
    gs = stages["pre_raster_gs"]
    ps = stages["pixel"]
    user_map = {value for value in gs["user_data_reg_map"]
                if value != 0xFFFFFFFF}
    base_vertex = 0x10000003 in user_map
    base_instance = 0x10000004 in user_map
    draw_index = 0x10000005 in user_map
    if not (base_vertex and base_instance) or draw_index:
        raise ValueError("unexpected DrawIndexAuto user-data ABI")
    draw_modifier = bit(base_vertex) | (bit(base_instance) << 2) | \
                    (bit(draw_index) << 3)

    onchip = graphics[".vgt_gs_onchip_cntl"]
    subgroup = graphics[".ge_ngg_subgrp_cntl"]
    stages_en = graphics[".vgt_shader_stages_en"]
    db = graphics[".db_shader_control"]
    ps_addr = graphics[".spi_ps_input_addr"]
    ps_ena = graphics[".spi_ps_input_ena"]
    bary = graphics[".spi_baryc_cntl"]

    def ps_inputs(fields: dict[str, object]) -> int:
        names = (
            ".persp_sample_ena", ".persp_center_ena", ".persp_centroid_ena",
            ".persp_pull_model_ena", ".linear_sample_ena", ".linear_center_ena",
            ".linear_centroid_ena", ".line_stipple_tex_ena", ".pos_x_float_ena",
            ".pos_y_float_ena", ".pos_z_float_ena", ".pos_w_float_ena",
            ".front_face_ena", ".ancillary_ena", ".sample_coverage_ena",
            ".pos_fixed_pt_ena",
        )
        return sum(bit(fields.get(name, False)) << index
                   for index, name in enumerate(names))

    pre_cx = [
        (0x1FF, graphics[".max_verts_per_subgroup"] & 0x3FF),
        (0x2D3, (subgroup[".prim_amp_factor"] & 0x1FF) |
                ((subgroup[".threads_per_subgroup"] & 0x1FF) << 9)),
        (0x207, 0),
        (0x1C2, graphics[".spi_shader_idx_format"] & 0xF),
        (0x1C3, pack(graphics[".spi_shader_pos_format"], 4)),
        (0x1B1, bit(graphics[".spi_vs_out_config"].get(".no_pc_export")) << 7),
        (0x2AB, graphics[".vgt_esgs_ring_itemsize"] & 0x7FFF),
        (0x2E4, 0),
        (0x2CE, graphics[".vgt_gs_max_vert_out"] & 0x7FF),
        (0x291, (onchip[".es_verts_per_subgroup"] & 0x7FF) |
                ((onchip[".gs_prims_per_subgroup"] & 0x7FF) << 11) |
                ((onchip[".gs_inst_prims_per_subgrp"] & 0x3FF) << 22)),
    ]
    db_value = (
        bit(db.get(".z_export_enable")) |
        (bit(db.get(".stencil_test_val_export_enable")) << 1) |
        ((db[".z_order"] & 3) << 4) |
        (bit(db.get(".kill_enable")) << 6) |
        (bit(db.get(".mask_export_enable")) << 8) |
        (bit(db.get(".exec_on_hier_fail")) << 9) |
        (bit(db.get(".exec_on_noop")) << 10) |
        (bit(db.get(".alpha_to_mask_disable")) << 11) |
        (bit(db.get(".depth_before_shader")) << 12) |
        ((db[".conservative_z_export"] & 3) << 13) |
        (bit(db.get(".primitive_ordered_pixel_shader")) << 16) |
        (bit(db.get(".pre_shader_depth_coverage_enable")) << 23)
    )
    pixel_cx = [
        (0x08F, pack(list(graphics[".cb_shader_mask"].values()), 4)),
        (0x203, db_value),
        (0x310, (graphics[".pa_sc_shader_control"][".wave_break_region_size"] & 3) << 5),
        (0x1B8, ((bit(bary[".pos_float_location"]) & 3) << 16) |
                (bit(bary[".front_face_all_bits"]) << 24)),
        (0x1B4, ps_inputs(ps_addr)),
        (0x1B3, ps_inputs(ps_ena)),
        (0x1B6, (graphics[".spi_ps_in_control"][".num_interps"] & 0x3F) |
                (bit(ps["wavefront_size"] == 32) << 15)),
        (0x1C5, pack(list(graphics[".spi_shader_col_format"].values()), 4)),
        (0x1C4, 0),
    ]

    def rsrc1(stage: dict[str, object], wave32: bool, component: int,
              gs_stage: bool) -> int:
        vgprs = 0 if stage["vgpr_count"] == 0 else \
            (stage["vgpr_count"] - 1) // (8 if wave32 else 4)
        sgprs = (stage["sgpr_count"] - 1) // 8
        value = vgprs | (sgprs << 6) | (192 << 12) | (1 << 21) | (1 << 25)
        if gs_stage:
            value |= bit(stage.get("wgp_mode")) << 27 | ((component & 3) << 29)
        return value

    stage_word = (
        ((stages_en.get(".es_stage_en", 0) & 3) << 3) |
        (bit(stages_en.get(".gs_stage_en")) << 5) |
        ((stages_en.get(".vs_stage_en", 0) & 3) << 6) |
        (bit(stages_en.get(".primgen_en")) << 13) |
        ((stages_en.get(".max_primgroup_in_wave", 0) & 0xF) << 15) |
        (bit(stages_en.get(".gs_w32_en")) << 22) |
        (bit(stages_en.get(".vs_w32_en")) << 23) |
        (bit(stages_en.get(".primgen_passthru_en")) << 25)
    )
    return {
        "gs_isa_bytes": manifest["stages"]["pre_raster_gs"]["bytes"],
        "ps_isa_bytes": manifest["stages"]["pixel"]["bytes"],
        "gs_rsrc1": rsrc1(gs, True, graphics[".gs_vgpr_comp_cnt"], True),
        "gs_rsrc2": ((gs["user_sgprs"] & 0x1F) << 1) |
                    ((graphics[".es_vgpr_comp_cnt"] & 3) << 16),
        "ps_rsrc1": rsrc1(ps, False, 0, False),
        "ps_rsrc2": (ps["user_sgprs"] & 0x1F) << 1,
        "ge_cntl": (onchip[".gs_prims_per_subgroup"] & 0x1FF) |
                   ((onchip[".es_verts_per_subgroup"] & 0x1FF) << 9),
        "shader_stages_en": stage_word,
        "draw_modifier": draw_modifier,
        "pre_raster_cx": pre_cx,
        "pixel_cx": pixel_cx,
    }


def render_header(values: dict[str, object]) -> str:
    def rows(name: str, registers: list[tuple[int, int]]) -> str:
        body = ",\n".join(
            f"    {{{offset:#05x}u, {value:#010x}u}}"
            for offset, value in registers
        )
        return f"static const ps5_agc_register {name}[] = {{\n{body}\n}};"

    return f"""#ifndef PS5_AGC_GEARS_SHADER_METADATA_H
#define PS5_AGC_GEARS_SHADER_METADATA_H
#include \"ps5_agc.h\"
/* Generated solely from the project-owned gfx1013 LLPC/PAL ELF. */
#define PS5_GEARS_GS_ISA_BYTES {values['gs_isa_bytes']}u
#define PS5_GEARS_PS_ISA_BYTES {values['ps_isa_bytes']}u
#define PS5_GEARS_GS_RSRC1 {values['gs_rsrc1']:#010x}u
#define PS5_GEARS_GS_RSRC2 {values['gs_rsrc2']:#010x}u
#define PS5_GEARS_PS_RSRC1 {values['ps_rsrc1']:#010x}u
#define PS5_GEARS_PS_RSRC2 {values['ps_rsrc2']:#010x}u
#define PS5_GEARS_GE_CNTL {values['ge_cntl']:#010x}u
#define PS5_GEARS_SHADER_STAGES_EN {values['shader_stages_en']:#010x}u
#define PS5_GEARS_GS_OUT_PRIM_TYPE 0x00000002u
#define PS5_GEARS_DRAW_MODIFIER {values['draw_modifier']:#018x}ull
{rows('ps5_gears_pre_raster_cx', values['pre_raster_cx'])}
{rows('ps5_gears_pixel_cx', values['pixel_cx'])}
#endif
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path,
                        default=ROOT / "build/shaders/gears_lit.manifest.json")
    parser.add_argument("--output", type=Path,
                        default=ROOT / "build/generated/gears_shader_metadata.h")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    values = derive(manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render_header(values), encoding="utf-8")
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
