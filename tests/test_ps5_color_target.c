#include "../src/ps5_color_target.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    static const uint32_t offsets[PS5_COLOR_REGISTER_COUNT] = {
        0x318, 0x31b, 0x31c, 0x31d, 0x31e, 0x31f, 0x321, 0x323,
        0x324, 0x325, 0x390, 0x398, 0x3a0, 0x3a8, 0x3b0, 0x3b8,
    };
    ps5_agc_register block[PS5_COLOR_REGISTER_COUNT];
    ps5_agc_register *cx[PS5_FW_1202_CX_DEFAULT_COUNT] = {0};
    struct ps5_agc_type_index types[PS5_FW_1202_DEFAULT_COUNT] = {0};
    struct ps5_agc_register_defaults root = {0};
    ps5_agc_register selected[PS5_COLOR_REGISTER_COUNT];
    ps5_agc_register target[PS5_COLOR_REGISTER_COUNT];
    for (uint32_t i = 0; i < PS5_COLOR_REGISTER_COUNT; ++i)
        block[i] = (ps5_agc_register){offsets[i], UINT32_C(0x12020000) | i};
    cx[72] = block;
    types[31] = (struct ps5_agc_type_index){UINT32_C(0x38e92c91), 72u << 2};
    root.table_cx = cx;
    root.type_index_pairs = types;
    root.count = PS5_FW_1202_DEFAULT_COUNT;
    assert(ps5_color_select_runtime_defaults(selected, &root) == 0);
    assert(memcmp(selected, block, sizeof(block)) == 0);
    assert(ps5_color_build_target(target, selected,
                                  UINT64_C(0x0000123456000000),
                                  1920u, 1080u) == 0);
    assert(target[0].value == UINT32_C(0x34560000));
    assert(target[10].value ==
           ((selected[10].value & UINT32_C(0xffffff00)) | 0x12u));
    assert(target[14].value == UINT32_C(0x01dfc437));

    root.count--;
    assert(ps5_color_select_runtime_defaults(selected, &root) == -1);
    root.count++;
    types[31].encoded_index |= 1u;
    assert(ps5_color_select_runtime_defaults(selected, &root) == -2);
    types[31].encoded_index = 72u << 2;
    types[32] = types[31];
    assert(ps5_color_select_runtime_defaults(selected, &root) == -3);
    types[32] = (struct ps5_agc_type_index){0u, 0u};
    block[15].offset++;
    assert(ps5_color_select_runtime_defaults(selected, &root) == -4);
    block[15].offset--;
    assert(ps5_color_build_target(target, block,
                                  UINT64_C(0x123456000001),
                                  1920u, 1080u) == -1);
    return 0;
}
