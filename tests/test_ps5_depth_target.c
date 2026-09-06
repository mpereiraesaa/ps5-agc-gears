#include "../src/ps5_depth_target.h"

#include <assert.h>

int main(void)
{
    ps5_agc_register regs[PS5_DEPTH_REGISTER_COUNT];
    const uintptr_t address = UINT64_C(0x0000123456000000);
    assert(ps5_depth_build_d32_no_htile(regs, address, 1920u, 1080u) == 0);
    assert(PS5_DEPTH_VIEW_REGISTER_COUNT == 21);
    assert(regs[0].offset == 0x000u && regs[0].value == 0x60u);
    assert(regs[5].offset == 0x007u && regs[5].value == 0x0437077fu);
    assert(regs[6].value == 0x20000180u);
    assert(regs[7].value == (uint32_t)(address >> 8));
    assert(regs[14].value == ((address >> 40) & 0xffu));
    assert(regs[20].value == 0x183u);
    assert(regs[21].offset == 0x200u && regs[21].value == 0xb6u);
    assert(ps5_depth_build_d32_no_htile(regs, address + 1u,
                                        1920u, 1080u) == -1);
    assert(ps5_depth_build_d32_no_htile(regs, address, 0u, 1080u) == -1);
    ps5_agc_register disabled[PS5_DEPTH_DISABLED_REGISTER_COUNT];
    assert(ps5_depth_build_disabled(disabled) == 0);
    assert(disabled[0].offset == 0x200u && disabled[0].value == 0u);
    assert(ps5_depth_build_disabled(0) == -1);
    return 0;
}
