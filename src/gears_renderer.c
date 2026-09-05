#include "gears_renderer.h"

#include <stddef.h>

enum { GEARS_RENDERER_FRAME_DWORDS = 120 };

int gears_renderer_compose_frame(
    uint32_t **cursor, uint32_t *end, const GearsSceneDraw *clear,
    const GearsSceneDraw gears[GEARS_SCENE_DRAW_COUNT], uint64_t modifier,
    GearsSetShDirectFn set_sh_direct, GearsDrawAutoFn draw_auto,
    GearsRendererComposeResult *result)
{
    if (!cursor || !*cursor || !end || *cursor > end || !clear || !gears ||
        !modifier || !set_sh_direct || !draw_auto || !result)
        return -1;
    uint32_t *const start = *cursor;
    GearsDrawComposeResult clear_result = {0};
    if (gears_rt_clear_compose(cursor, end, clear, modifier, set_sh_direct,
                               draw_auto, &clear_result) != 0)
        return -2;
    GearsDrawComposeResult gears_result = {0};
    if (gears_compose_three_draws(cursor, end, gears, modifier, set_sh_direct,
                                  draw_auto, &gears_result) != 0)
        return -3;
    result->draws = clear_result.draws + gears_result.draws;
    result->command_dwords = (uint32_t)(*cursor - start);
    return result->draws == 4 &&
                   result->command_dwords == GEARS_RENDERER_FRAME_DWORDS
               ? 0 : -4;
}
