#ifndef PS5_AGC_GEARS_BSP_RESOURCE_DRAW_H
#define PS5_AGC_GEARS_BSP_RESOURCE_DRAW_H

#include "bsp_resource_frame.h"
#include "bsp_flat_draw.h"

enum {
    /* AGC direct user-data offsets are stage-banked, not interchangeable. */
    BSP_RESOURCE_GS_SH_OFFSET = 0x8d,
    BSP_RESOURCE_PS_SH_OFFSET = 0x0d,
};

enum bsp_resource_draw_class {
    BSP_RESOURCE_DRAW_OPAQUE = 0,
    BSP_RESOURCE_DRAW_ALPHA_TEST = 1,
    BSP_RESOURCE_DRAW_ALL = 2,
};

typedef struct BspResourceComposeResult {
    uint32_t map_draws;
    uint32_t opaque_draws;
    uint32_t alpha_test_draws;
    uint32_t overlay_draws;
    uint32_t command_dwords;
} BspResourceComposeResult;

int bsp_resource_draw_counts(const BspBundleView *bundle,
                             uint32_t *opaque_draws,
                             uint32_t *alpha_test_draws);

int bsp_resource_compose_map_pass(
    uint32_t **cursor, uint32_t *end, const BspResourceFrame *frame,
    const BspBundleView *bundle, const uint16_t clear_indices[3],
    enum bsp_resource_draw_class draw_class, int include_clear,
    const void *gpu_mapping, size_t gpu_mapping_bytes, uint64_t modifier,
    BspSetShDirectFn set_sh_direct, BspDrawIndexedFn draw_indexed,
    BspResourceComposeResult *result);

int bsp_resource_compose_map(
    uint32_t **cursor, uint32_t *end, const BspResourceFrame *frame,
    const BspBundleView *bundle, const uint16_t clear_indices[3],
    const void *gpu_mapping, size_t gpu_mapping_bytes, uint64_t modifier,
    BspSetShDirectFn set_sh_direct, BspDrawIndexedFn draw_indexed,
    BspResourceComposeResult *result);

int bsp_resource_compose_overlay(
    uint32_t **cursor, uint32_t *end, const BspResourceFrame *frame,
    const void *gpu_mapping, size_t gpu_mapping_bytes, uint64_t modifier,
    BspSetShDirectFn set_sh_direct, BspDrawIndexedFn draw_indexed,
    BspResourceComposeResult *result);

#endif
