#include "gears_rt_clear.h"

#include <stddef.h>
#include <string.h>

enum {
    GEARS_PARAMETER_SH_OFFSET = 0x8d,
    GEARS_SH_DIRECT_DWORDS = 27,
    GEARS_DRAW_AUTO_DWORDS = 3,
    GEARS_RT_CLEAR_DWORDS = 30,
};

static uint32_t bits(float value)
{
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

int gears_rt_clear_build(GearsRtClearVertex vertices[GEARS_RT_CLEAR_VERTEX_COUNT],
                         GearsSceneDraw *draw, uint32_t table,
                         const float rgba[4])
{
    if (!vertices || !draw || !table || !rgba) return -1;
    static const float positions[3][3] = {
        {-1.0f, -1.0f, 1.0f}, {3.0f, -1.0f, 1.0f}, {-1.0f, 3.0f, 1.0f},
    };
    memset(draw, 0, sizeof(*draw));
    for (unsigned i = 0; i < 3; ++i) {
        memcpy(vertices[i].position, positions[i], sizeof(positions[i]));
        vertices[i].normal[2] = 1.0f;
    }
    for (unsigned i = 0; i < 16; ++i)
        draw->sh_words[i] = bits((i % 5) == 0 ? 1.0f : 0.0f);
    draw->sh_words[19] = bits(1.0f); /* identity quaternion w */
    for (unsigned i = 0; i < 4; ++i)
        draw->sh_words[20 + i] = bits(rgba[i]);
    draw->sh_words[24] = table;
    draw->vertex_count = GEARS_RT_CLEAR_VERTEX_COUNT;
    return 0;
}

int gears_rt_clear_compose(uint32_t **cursor, uint32_t *end,
                           const GearsSceneDraw *draw, uint64_t modifier,
                           GearsSetShDirectFn set_sh_direct,
                           GearsDrawAutoFn draw_auto,
                           GearsDrawComposeResult *result)
{
    if (!cursor || !*cursor || !end || *cursor > end || !draw || !modifier ||
        !set_sh_direct || !draw_auto || !result ||
        draw->vertex_count != GEARS_RT_CLEAR_VERTEX_COUNT)
        return -1;
    uint32_t *const start = *cursor;
    uint32_t *before = *cursor;
    if (set_sh_direct(cursor, (uint32_t)(end - *cursor),
                      GEARS_PARAMETER_SH_OFFSET, draw->sh_words,
                      GEARS_SCENE_SH_WORDS) != 0 ||
        *cursor - before != GEARS_SH_DIRECT_DWORDS)
        return -2;
    before = *cursor;
    if (draw_auto(cursor, (uint32_t)(end - *cursor), draw->vertex_count,
                  modifier) != 0 || *cursor - before != GEARS_DRAW_AUTO_DWORDS)
        return -3;
    result->draws = 1;
    result->command_dwords = (uint32_t)(*cursor - start);
    return result->command_dwords == GEARS_RT_CLEAR_DWORDS ? 0 : -4;
}
