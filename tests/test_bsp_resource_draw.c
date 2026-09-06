#include "../src/bsp_resource_draw.h"

#include <assert.h>
#include <stdint.h>

static uint32_t *mapping;
static size_t mapping_bytes;
static uint32_t gs_updates;
static uint32_t ps_updates;

static int set_sh(uint32_t **cursor, uint32_t capacity, uint32_t offset,
                  const uint32_t *values, uint32_t count)
{
    assert(cursor && capacity >= count + 2u && values);
    if (offset == BSP_RESOURCE_GS_SH_OFFSET)
        ++gs_updates;
    else {
        assert(offset == BSP_RESOURCE_PS_SH_OFFSET);
        ++ps_updates;
    }
    *cursor += count + 2u;
    return 0;
}

static int draw(uint32_t **cursor, uint32_t capacity, uint32_t count,
                const uint16_t *indices, const void *gpu_mapping,
                size_t gpu_mapping_bytes, uint64_t modifier)
{
    assert(cursor && capacity >= 6u && count % 3u == 0u && indices);
    assert(gpu_mapping == mapping && gpu_mapping_bytes == mapping_bytes);
    assert(modifier == 7u);
    *cursor += 6u;
    return 0;
}

int main(void)
{
    _Alignas(16) uint32_t gpu[512] = {0};
    mapping = gpu;
    mapping_bytes = sizeof(gpu);
    BspResourceFrame frame = {
        .map_constant_table = gpu + 0,
        .clear_constant_table = gpu + 4,
        .map_vertex_table = gpu + 8,
        .clear_vertex_table = gpu + 12,
        .texture_tables = gpu + 16,
        .texture_table_dwords = 24u,
        .overlay_constant_table = gpu + 40,
        .overlay_indices = (const uint16_t *)(gpu + 44),
        .overlay_index_count = 6u,
    };
    BspBundleDraw source = {
        .first_index = 0u, .index_count = 3u, .base_texture = 0u,
    };
    BspBundleView bundle = {
        .draws = &source, .draw_count = 1u,
        .indices = (const uint16_t *)(gpu + 48), .index_count = 3u,
        .texture_count = 1u,
    };
    uint32_t commands[64] = {0};
    uint32_t *cursor = commands;
    BspResourceComposeResult result = {0};
    assert(bsp_resource_compose_map(
               &cursor, commands + 64, &frame, &bundle,
               (const uint16_t *)(gpu + 52), gpu, sizeof(gpu), 7u,
               set_sh, draw, &result) == 0);
    assert(result.map_draws == 1u && result.command_dwords == 26u);
    assert(gs_updates == 2u && ps_updates == 2u);
    assert(bsp_resource_compose_overlay(
               &cursor, commands + 64, &frame, gpu, sizeof(gpu), 7u,
               set_sh, draw, &result) == 0);
    assert(result.overlay_draws == 1u && result.command_dwords == 35u);
    assert(gs_updates == 3u && ps_updates == 2u);
    source.base_texture = 1u;
    cursor = commands;
    assert(bsp_resource_compose_map(
               &cursor, commands + 64, &frame, &bundle,
               (const uint16_t *)(gpu + 52), gpu, sizeof(gpu), 7u,
               set_sh, draw, &result) == -3);
    return 0;
}
