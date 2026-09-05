#ifndef PS5_AGC_GEARS_COLOR_TARGET_H
#define PS5_AGC_GEARS_COLOR_TARGET_H

#include "../include/ps5_agc.h"
#include "../include/ps5_agc_registers.h"

enum {
    PS5_COLOR_REGISTER_COUNT = 16,
    PS5_FW_1202_DEFAULT_COUNT = 137,
    PS5_FW_1202_CX_DEFAULT_COUNT = 84,
};

int ps5_color_select_runtime_defaults(
    ps5_agc_register out[PS5_COLOR_REGISTER_COUNT],
    const struct ps5_agc_register_defaults *root);
int ps5_color_build_target(
    ps5_agc_register out[PS5_COLOR_REGISTER_COUNT],
    const ps5_agc_register defaults[PS5_COLOR_REGISTER_COUNT],
    uintptr_t address, uint32_t width, uint32_t height);

#endif
