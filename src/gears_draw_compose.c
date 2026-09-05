#include "gears_draw_compose.h"

#include <stddef.h>

enum {
    GEARS_PARAMETER_SH_OFFSET = 0x8d,
    GEARS_SH_DIRECT_DWORDS = 27,
    GEARS_DRAW_AUTO_DWORDS = 3,
    GEARS_THREE_DRAW_DWORDS = 90,
};

int gears_compose_three_draws(
    uint32_t **cursor, uint32_t *end,
    const GearsSceneDraw draws[GEARS_SCENE_DRAW_COUNT], uint64_t modifier,
    GearsSetShDirectFn set_sh_direct, GearsDrawAutoFn draw_auto,
    GearsDrawComposeResult *result)
{
    if (!cursor || !*cursor || !end || *cursor > end || !draws || !modifier ||
        !set_sh_direct || !draw_auto || !result)
        return -1;
    uint32_t *start = *cursor;
    result->draws = 0;
    result->command_dwords = 0;
    for (uint32_t gear = 0; gear < GEARS_SCENE_DRAW_COUNT; ++gear) {
        if (!draws[gear].vertex_count) return -2;
        uint32_t *before = *cursor;
        if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                          GEARS_PARAMETER_SH_OFFSET, draws[gear].sh_words,
                          GEARS_SCENE_SH_WORDS) != 0 ||
            *cursor - before != GEARS_SH_DIRECT_DWORDS)
            return -3;
        before = *cursor;
        if (draw_auto(cursor, (uint32_t)(end - *cursor),
                      draws[gear].vertex_count, modifier) != 0 ||
            *cursor - before != GEARS_DRAW_AUTO_DWORDS)
            return -4;
        ++result->draws;
    }
    result->command_dwords = (uint32_t)(*cursor - start);
    return result->command_dwords == GEARS_THREE_DRAW_DWORDS ? 0 : -5;
}
