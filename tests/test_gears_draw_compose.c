#include <assert.h>
#include <string.h>

#include "../src/gears_draw_compose.h"

static int fake_sh(uint32_t **cursor, uint32_t capacity, uint32_t offset,
                   const uint32_t *values, uint32_t count)
{
    assert(offset == 0x8d && values && count == 25);
    if (capacity < 27) return -1;
    *cursor += 27;
    return 0;
}

static int fake_draw(uint32_t **cursor, uint32_t capacity,
                     uint32_t vertices, uint64_t modifier)
{
    assert(vertices && modifier == 5);
    if (capacity < 3) return -1;
    *cursor += 3;
    return 0;
}

static int wrong_draw_size(uint32_t **cursor, uint32_t capacity,
                           uint32_t vertices, uint64_t modifier)
{
    (void)capacity; (void)vertices; (void)modifier;
    *cursor += 2;
    return 0;
}

int main(void)
{
    uint32_t words[128] = {0};
    uint32_t *cursor = words;
    GearsSceneDraw draws[3];
    memset(draws, 0, sizeof(draws));
    draws[0].vertex_count = 1200;
    draws[1].vertex_count = draws[2].vertex_count = 600;
    GearsDrawComposeResult result;
    assert(gears_compose_three_draws(&cursor, words + 128, draws, 5,
                                     fake_sh, fake_draw, &result) == 0);
    assert(result.draws == 3 && result.command_dwords == 90);
    assert(cursor == words + 90);
    cursor = words;
    assert(gears_compose_three_draws(&cursor, words + 128, draws, 5,
                                     fake_sh, wrong_draw_size, &result) == -4);
    cursor = words;
    assert(gears_compose_three_draws(&cursor, words + 82, draws, 5,
                                     fake_sh, fake_draw, &result) == -3);
    return 0;
}
