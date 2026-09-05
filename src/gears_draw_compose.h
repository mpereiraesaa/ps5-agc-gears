#ifndef PS5_AGC_GEARS_DRAW_COMPOSE_H
#define PS5_AGC_GEARS_DRAW_COMPOSE_H

#include <stdint.h>

#include "gears_scene.h"

typedef int (*GearsSetShDirectFn)(uint32_t **cursor, uint32_t capacity,
                                  uint32_t offset, const uint32_t *values,
                                  uint32_t count);
typedef int (*GearsDrawAutoFn)(uint32_t **cursor, uint32_t capacity,
                               uint32_t vertex_count, uint64_t modifier);

typedef struct GearsDrawComposeResult {
    uint32_t draws;
    uint32_t command_dwords;
} GearsDrawComposeResult;

/* FW 12.02 contract: three (25-word SH-direct + 3-DWORD DrawIndexAuto) pairs. */
int gears_compose_three_draws(
    uint32_t **cursor, uint32_t *end,
    const GearsSceneDraw draws[GEARS_SCENE_DRAW_COUNT], uint64_t modifier,
    GearsSetShDirectFn set_sh_direct, GearsDrawAutoFn draw_auto,
    GearsDrawComposeResult *result);

#endif
