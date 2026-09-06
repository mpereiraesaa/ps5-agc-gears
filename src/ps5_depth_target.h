#ifndef PS5_AGC_GEARS_DEPTH_TARGET_H
#define PS5_AGC_GEARS_DEPTH_TARGET_H

#include "../include/ps5_agc.h"

enum {
    PS5_DEPTH_VIEW_REGISTER_COUNT = 21,
    PS5_DEPTH_REGISTER_COUNT = 22,
};

int ps5_depth_build_d32_no_htile(
    ps5_agc_register out[PS5_DEPTH_REGISTER_COUNT], uintptr_t address,
    uint32_t width, uint32_t height);

#endif
