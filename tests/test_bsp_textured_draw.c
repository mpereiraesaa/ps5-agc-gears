#include "../src/bsp_textured_draw.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static uint32_t calls;

static int set_sh(uint32_t **cursor, uint32_t capacity, uint32_t offset,
                  const uint32_t *values, uint32_t count)
{
    assert(cursor && *cursor && values);
    assert(capacity >= count + 2u);
    assert((calls & 1u) == 0u ? offset == 0x8du && count == 21u
                              : offset == 0x0du && count == 1u);
    (*cursor)[0] = offset;
    (*cursor)[1] = count;
    memcpy(*cursor + 2u, values, count * sizeof(uint32_t));
    *cursor += count + 2u;
    ++calls;
    return 0;
}

static int draw_indexed(uint32_t **cursor, uint32_t capacity,
                        uint32_t index_count, const uint16_t *indices,
                        const void *mapping, size_t mapping_bytes,
                        uint64_t modifier)
{
    assert(cursor && *cursor && capacity >= 6u && index_count == 3u);
    assert(indices && mapping && mapping_bytes && modifier == 5u);
    *cursor += 6u;
    return 0;
}

int main(void)
{
    union {
        uint64_t alignment;
        uint8_t bytes[1024];
    } gpu;
    memset(&gpu, 0, sizeof(gpu));
    BspFlatDraw draws[2] = {0};
    const uint32_t *tables[2] = {
        (const uint32_t *)(gpu.bytes + 256u),
        (const uint32_t *)(gpu.bytes + 352u),
    };
    draws[0].index_count = draws[1].index_count = 3u;
    draws[0].indices = (const uint16_t *)(gpu.bytes + 640u);
    draws[1].indices = (const uint16_t *)(gpu.bytes + 648u);
    draws[0].sh_words[0] = 0x11111111u;
    draws[1].sh_words[0] = 0x22222222u;
    uint32_t required = 0u;
    assert(bsp_textured_required_dwords(2u, &required) == 0);
    assert(required == 64u);
    uint32_t command[64] = {0};
    uint32_t *cursor = command;
    BspFlatComposeResult result;
    assert(bsp_textured_compose(
               &cursor, command + 63u, draws, tables, 2u, gpu.bytes,
               sizeof(gpu.bytes), 5u, set_sh, draw_indexed, &result) == -2);
    assert(cursor == command);
    calls = 0u;
    assert(bsp_textured_compose(
               &cursor, command + 64u, draws, tables, 2u, gpu.bytes,
               sizeof(gpu.bytes), 5u, set_sh, draw_indexed, &result) == 0);
    assert(cursor == command + 64u && result.draws == 2u &&
           result.command_dwords == 64u && calls == 4u);
    assert(command[0] == 0x8du && command[23] == 0x0du);
    assert(command[25] == (uint32_t)(uintptr_t)tables[0]);
    assert(command[32] == 0x8du && command[55] == 0x0du);
    assert(command[57] == (uint32_t)(uintptr_t)tables[1]);

    const uint32_t *bad_tables[2] = {tables[0],
        (const uint32_t *)(gpu.bytes + 944u)};
    cursor = command;
    assert(bsp_textured_compose(
               &cursor, command + 64u, draws, bad_tables, 2u, gpu.bytes,
               sizeof(gpu.bytes), 5u, set_sh, draw_indexed, &result) == -3);
    assert(cursor == command);
    return 0;
}
