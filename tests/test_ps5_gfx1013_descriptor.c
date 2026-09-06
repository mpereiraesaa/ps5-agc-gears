#include "../src/ps5_gfx1013_descriptor.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    uint32_t vsharp[PS5_GFX1013_VSHARP_DWORDS] = {0};
    assert(ps5_gfx1013_build_vsharp(
               vsharp, UINT64_C(0x0000000212345600), 32u, 123u) == 0);
    const uint32_t expected_v[] = {
        UINT32_C(0x12345600), UINT32_C(0x00200002), 123u,
        UINT32_C(0x11014fac),
    };
    assert(memcmp(vsharp, expected_v, sizeof(expected_v)) == 0);
    assert(ps5_gfx1013_build_vsharp(
               vsharp, UINT64_C(0x0000000212345601), 32u, 123u) != 0);
    assert(ps5_gfx1013_build_constant_vsharp(
               vsharp, UINT64_C(0x0000000212345600), 125u) == 0);
    const uint32_t expected_constant[] = {
        UINT32_C(0x12345600), UINT32_C(0x00000002), 128u,
        UINT32_C(0x31016fac),
    };
    assert(memcmp(vsharp, expected_constant, sizeof(expected_constant)) == 0);

    uint32_t tsharp[PS5_GFX1013_TSHARP_DWORDS];
    assert(ps5_gfx1013_build_tsharp_rgba8(
               tsharp, UINT64_C(0x0000123456789a00),
               64u, 32u, 256u) == 0);
    const uint32_t expected_t[] = {
        UINT32_C(0x3456789a), UINT32_C(0xc3800012),
        UINT32_C(0x8007c00f), UINT32_C(0x90000fac),
        0u, UINT32_C(0x00400000), 0u, 0u,
    };
    assert(memcmp(tsharp, expected_t, sizeof(expected_t)) == 0);

    assert(ps5_gfx1013_build_tsharp_rgba8_mip(
               tsharp, UINT64_C(0x0000123456789a00),
               64u, 32u, 256u, 7u) == 0);
    const uint32_t expected_mip_t[] = {
        UINT32_C(0x3456789a), UINT32_C(0xc3800012),
        UINT32_C(0x8007c00f), UINT32_C(0x90060fac),
        0u, UINT32_C(0x00400060), 0u, 0u,
    };
    assert(memcmp(tsharp, expected_mip_t, sizeof(expected_mip_t)) == 0);
    assert(ps5_gfx1013_build_tsharp_rgba8_mip(
               tsharp, UINT64_C(0x0000123456789a00),
               64u, 32u, 512u, 7u) != 0);

    uint32_t ssharp[PS5_GFX1013_SSHARP_DWORDS];
    assert(ps5_gfx1013_build_ssharp(
               ssharp, PS5_GFX1013_CLAMP_LAST_TEXEL,
               PS5_GFX1013_FILTER_BILINEAR) == 0);
    const uint32_t expected_s[] = {
        UINT32_C(0x92), 0u, UINT32_C(0x00500000), 0u,
    };
    assert(memcmp(ssharp, expected_s, sizeof(expected_s)) == 0);
    assert(ps5_gfx1013_build_ssharp_mip(
               ssharp, PS5_GFX1013_REPEAT,
               PS5_GFX1013_FILTER_TRILINEAR, 7u) == 0);
    const uint32_t expected_trilinear[] = {
        0u, UINT32_C(0x00600000), UINT32_C(0x08500000), 0u,
    };
    assert(memcmp(ssharp, expected_trilinear,
                  sizeof(expected_trilinear)) == 0);
    assert(ps5_gfx1013_build_ssharp_mip(
               ssharp, PS5_GFX1013_REPEAT,
               PS5_GFX1013_FILTER_ANISOTROPIC_4X, 7u) == 0);
    const uint32_t expected_aniso[] = {
        UINT32_C(0x00410400), UINT32_C(0x08600000),
        UINT32_C(0x28f00000), 0u,
    };
    assert(memcmp(ssharp, expected_aniso, sizeof(expected_aniso)) == 0);
    assert(ps5_gfx1013_build_ssharp_mip(
               ssharp, PS5_GFX1013_REPEAT,
               PS5_GFX1013_FILTER_TRILINEAR, 1u) != 0);
    return 0;
}
