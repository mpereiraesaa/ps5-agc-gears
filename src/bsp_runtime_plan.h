#ifndef PS5_AGC_GEARS_BSP_RUNTIME_PLAN_H
#define PS5_AGC_GEARS_BSP_RUNTIME_PLAN_H

#include <stddef.h>
#include <stdint.h>

enum {
    BSP_RUNTIME_ALLOCATION_ALIGNMENT = 0x10000,
    BSP_RUNTIME_SRD_ALIGNMENT = 256,
};

typedef struct BspRuntimePlan {
    size_t bundle_bytes;
    size_t vertex_srds_offset;
    size_t clear_vertices_offset;
    size_t clear_indices_offset;
    size_t scene_draws_offset;
    size_t guard_offset;
    size_t allocation_bytes;
    uint32_t scene_draw_count;
} BspRuntimePlan;

/* The allocation keeps the validated file bytes in their final GPU-visible
 * location. One leading scene draw is reserved for a deterministic clear. */
int bsp_runtime_plan(size_t bundle_bytes, uint32_t map_draw_count,
                     BspRuntimePlan *plan);

#endif
