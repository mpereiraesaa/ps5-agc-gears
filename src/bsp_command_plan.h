#ifndef PS5_AGC_GEARS_BSP_COMMAND_PLAN_H
#define PS5_AGC_GEARS_BSP_COMMAND_PLAN_H

#include <stdint.h>

enum {
    BSP_COMMAND_SLOT_ALIGNMENT = 0x10000,
    BSP_COMMAND_FENCE_ALIGNMENT = 64,
};

typedef struct BspCommandPlan {
    uint32_t draw_dwords;
    uint32_t frame_capacity_dwords;
    uint32_t slot_bytes;
    uint32_t slot_offsets[2];
    uint32_t fence_offsets[2];
    uint32_t allocation_bytes;
} BspCommandPlan;

/* Plan two isolated streams and cache-line-separated fences. fixed_dwords is
 * the measured non-map packet reserve (wait, clears, pipeline and present). */
int bsp_command_plan(uint32_t draw_count, uint32_t fixed_dwords,
                     BspCommandPlan *plan);

#endif
