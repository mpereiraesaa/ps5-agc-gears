#include "ps5_color_target.h"

#include <string.h>

static const uint32_t color_offsets[PS5_COLOR_REGISTER_COUNT] = {
    0x318, 0x31b, 0x31c, 0x31d, 0x31e, 0x31f, 0x321, 0x323,
    0x324, 0x325, 0x390, 0x398, 0x3a0, 0x3a8, 0x3b0, 0x3b8,
};

int ps5_color_select_runtime_defaults(
    ps5_agc_register out[PS5_COLOR_REGISTER_COUNT],
    const struct ps5_agc_register_defaults *root)
{
    const uint32_t mrt0_key = UINT32_C(0x38e92c91);
    uint32_t match = UINT32_MAX;
    uint32_t matches = 0u;
    if (!out || !root || !root->table_cx || !root->type_index_pairs ||
        root->table_3 || root->count != PS5_FW_1202_DEFAULT_COUNT)
        return -1;
    for (uint32_t i = 0; i < root->count; ++i) {
        const struct ps5_agc_type_index *entry = &root->type_index_pairs[i];
        if (entry->key != mrt0_key)
            continue;
        if (ps5_agc_type_index_bank(entry) != 0u)
            return -2;
        match = ps5_agc_type_index_value(entry);
        ++matches;
    }
    if (matches != 1u || match >= PS5_FW_1202_CX_DEFAULT_COUNT ||
        !root->table_cx[match])
        return -3;
    for (uint32_t i = 0; i < PS5_COLOR_REGISTER_COUNT; ++i)
        if (root->table_cx[match][i].offset != color_offsets[i])
            return -4;
    memcpy(out, root->table_cx[match], sizeof(*out) * PS5_COLOR_REGISTER_COUNT);
    return 0;
}

int ps5_color_build_target(
    ps5_agc_register out[PS5_COLOR_REGISTER_COUNT],
    const ps5_agc_register defaults[PS5_COLOR_REGISTER_COUNT],
    uintptr_t address, uint32_t width, uint32_t height)
{
    if (!out || !defaults || !address || (address & UINT32_C(0x1ffff)) ||
        !width || !height || width > 0x3fffu || height > 0x3fffu)
        return -1;
    for (uint32_t i = 0; i < PS5_COLOR_REGISTER_COUNT; ++i)
        if (defaults[i].offset != color_offsets[i])
            return -2;
    memcpy(out, defaults, sizeof(*out) * PS5_COLOR_REGISTER_COUNT);
    out[0].value = (uint32_t)(address >> 8);
    out[1].value &= UINT32_C(0xfc001fff);
    out[2].value = (out[2].value & ~UINT32_C(0x1005df7c)) |
                   UINT32_C(0x00008028);
    out[3].value &= ~UINT32_C(0x0001f000);
    out[4].value = (out[4].value & ~UINT32_C(0x0018026c)) |
                   UINT32_C(0x00000048);
    out[5].value = out[6].value = out[9].value = 0u;
    out[10].value = (out[10].value & UINT32_C(0xffffff00)) |
                    (uint32_t)((address >> 40) & 0xffu);
    out[11].value &= UINT32_C(0xffffff00);
    out[12].value &= UINT32_C(0xffffff00);
    out[13].value &= UINT32_C(0xffffff00);
    out[14].value = (height - 1u) | ((width - 1u) << 14u);
    out[15].value = (out[15].value & ~UINT32_C(0x4707dfff)) |
                    UINT32_C(0x4506c000);
    return 0;
}
