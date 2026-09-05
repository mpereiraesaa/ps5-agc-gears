#ifndef PS5_AGC_GEARS_RT_CLEAR_H
#define PS5_AGC_GEARS_RT_CLEAR_H

#include <stdint.h>

#include "gears_draw_compose.h"

enum { GEARS_RT_CLEAR_VERTEX_COUNT = 3 };

typedef struct GearsRtClearVertex {
    float position[3];
    float normal[3];
} GearsRtClearVertex;

/* Builds an oversized fullscreen triangle at the far clip plane. */
int gears_rt_clear_build(GearsRtClearVertex vertices[GEARS_RT_CLEAR_VERTEX_COUNT],
                         GearsSceneDraw *draw, uint32_t srd_table_address,
                         const float rgba[4]);

/* Emits one 25-word SH update followed by DrawIndexAuto(3). */
int gears_rt_clear_compose(uint32_t **cursor, uint32_t *end,
                           const GearsSceneDraw *draw, uint64_t modifier,
                           GearsSetShDirectFn set_sh_direct,
                           GearsDrawAutoFn draw_auto,
                           GearsDrawComposeResult *result);

#endif
