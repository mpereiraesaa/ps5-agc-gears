#ifndef PS5_AGC_GEARS_RENDERER_H
#define PS5_AGC_GEARS_RENDERER_H

#include "gears_rt_clear.h"

typedef struct GearsRendererComposeResult {
    uint32_t draws;
    uint32_t command_dwords;
} GearsRendererComposeResult;

/* Public command core: one RT-clear draw followed by the three scene draws. */
int gears_renderer_compose_frame(
    uint32_t **cursor, uint32_t *end, const GearsSceneDraw *clear,
    const GearsSceneDraw gears[GEARS_SCENE_DRAW_COUNT], uint64_t modifier,
    GearsSetShDirectFn set_sh_direct, GearsDrawAutoFn draw_auto,
    GearsRendererComposeResult *result);

#endif
