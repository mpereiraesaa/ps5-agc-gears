#include "../src/bsp_flat_draw.h"

#include <assert.h>
#include <string.h>

static unsigned behavior;

static int direct(uint32_t **cursor, uint32_t capacity, uint32_t offset,
                  const uint32_t *values, uint32_t count)
{
    assert(capacity >= 23u && offset == 0x8du && values && count == 21u);
    *cursor += behavior == 1u ? 22u : 23u;
    return 0;
}

static int indexed(uint32_t **cursor, uint32_t capacity, uint32_t count,
                   const uint32_t *indices, const void *gpu_mapping,
                   size_t gpu_mapping_bytes, uint64_t modifier)
{
    assert(capacity >= 6u && count == 3u && indices && modifier == 5u);
    assert(gpu_mapping && gpu_mapping_bytes == 9u * sizeof(uint32_t));
    *cursor += behavior == 2u ? 5u : 6u;
    return 0;
}

int main(void)
{
    uint32_t commands[BSP_FLAT_DWORDS_PER_DRAW * 3] = {0};
    uint32_t indices[9] = {0};
    BspFlatDraw draws[3];
    memset(draws, 0, sizeof(draws));
    for (unsigned index = 0; index < 3u; ++index) {
        draws[index].indices = indices + index * 3u;
        draws[index].index_count = 3u;
    }
    uint32_t required = 0u;
    assert(bsp_flat_required_dwords(3u, &required) == 0 &&
           required == sizeof(commands) / sizeof(commands[0]));
    uint32_t *cursor = commands;
    BspFlatComposeResult result = {0};
    assert(bsp_flat_compose(&cursor, commands + required, draws, 3u,
                            indices, sizeof(indices), 5u,
                            direct, indexed, &result) == 0);
    assert(cursor == commands + required && result.draws == 3u &&
           result.command_dwords == required);
    cursor = commands;
    assert(bsp_flat_compose(&cursor, commands + required - 1u, draws, 3u,
                            indices, sizeof(indices), 5u, direct, indexed,
                            &result) == -2);
    assert(cursor == commands);
    behavior = 1u;
    assert(bsp_flat_compose(&cursor, commands + required, draws, 3u,
                            indices, sizeof(indices), 5u,
                            direct, indexed, &result) == -4);
    behavior = 2u;
    cursor = commands;
    assert(bsp_flat_compose(&cursor, commands + required, draws, 3u,
                            indices, sizeof(indices), 5u,
                            direct, indexed, &result) == -5);
    assert(bsp_flat_required_dwords(UINT32_MAX, &required) != 0);
    return 0;
}
