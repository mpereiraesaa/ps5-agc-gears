#include <assert.h>
#include <string.h>

#include "../src/gears_renderer.h"

static int set_sh(uint32_t **cursor, uint32_t capacity, uint32_t offset,
                  const uint32_t *values, uint32_t count)
{
    if (capacity < 27) return -1;
    assert(offset == 0x8d && values && count == 25);
    *cursor += 27;
    return 0;
}

static int draw(uint32_t **cursor, uint32_t capacity, uint32_t count,
                uint64_t modifier)
{
    if (capacity < 3) return -1;
    assert(count > 0 && modifier == 5);
    *cursor += 3;
    return 0;
}

int main(void)
{
    GearsSceneDraw clear = {{0}, 3};
    GearsSceneDraw gears[3] = {{{0}, 1200}, {{0}, 600}, {{0}, 600}};
    uint32_t commands[120] = {0};
    uint32_t *cursor = commands;
    GearsRendererComposeResult result;
    assert(gears_renderer_compose_frame(&cursor, commands + 120, &clear,
        gears, 5, set_sh, draw, &result) == 0);
    assert(cursor == commands + 120 && result.draws == 4 &&
           result.command_dwords == 120);
    cursor = commands;
    assert(gears_renderer_compose_frame(&cursor, commands + 119, &clear,
        gears, 5, set_sh, draw, &result) != 0);
    return 0;
}
