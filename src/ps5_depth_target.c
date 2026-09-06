#include "ps5_depth_target.h"

#include <stddef.h>

int ps5_depth_build_d32_no_htile(
    ps5_agc_register out[PS5_DEPTH_REGISTER_COUNT], uintptr_t address,
    uint32_t width, uint32_t height)
{
    if (!out || !address || (address & 0xffffu) || !width || !height ||
        width > 16384u || height > 16384u)
        return -1;
    const uint32_t lo = (uint32_t)(address >> 8);
    const uint32_t hi = (uint32_t)(address >> 40) & 0xffu;
    const ps5_agc_register plan[PS5_DEPTH_REGISTER_COUNT] = {
        {0x000, 0x00000060}, {0x01f, 0x00000000},
        {0x002, 0x00000000}, {0x004, 0x00000000},
        {0x005, 0x00000000},
        {0x007, ((height - 1u) << 16) | (width - 1u)},
        {0x011, 0x20000180}, {0x012, lo}, {0x013, 0x00000000},
        {0x014, lo}, {0x015, 0x00000000}, {0x2af, 0x00000000},
        {0x2de, 0x000001e9}, {0x092, 0x00000000}, {0x01a, hi},
        {0x01c, hi}, {0x01b, 0x00000000}, {0x01d, 0x00000000},
        {0x01e, 0x00000000}, {0x003, 0x0000002a},
        {0x010, 0x00000183}, {0x200, 0x000000b6},
    };
    for (size_t i = 0; i < PS5_DEPTH_REGISTER_COUNT; ++i)
        out[i] = plan[i];
    return 0;
}

int ps5_depth_build_disabled(
    ps5_agc_register out[PS5_DEPTH_DISABLED_REGISTER_COUNT])
{
    if (!out)
        return -1;
    out[0] = (ps5_agc_register){0x200, 0};
    return 0;
}
