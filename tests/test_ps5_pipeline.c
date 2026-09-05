#include "../src/ps5_pipeline.h"

#include <assert.h>
#include <string.h>

static uint64_t fnv1a64(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

int main(void)
{
    static const uint32_t offsets[16] = {
        0x318, 0x31b, 0x31c, 0x31d, 0x31e, 0x31f, 0x321, 0x323,
        0x324, 0x325, 0x390, 0x398, 0x3a0, 0x3a8, 0x3b0, 0x3b8,
    };
    ps5_agc_register rt[16];
    ps5_agc_register pre_sh[6] = {
        {0x80, 0}, {0x80, 0}, {0x8a, 0x002c0000},
        {0x8b, 0}, {0xc8, 1}, {0xc9, 2},
    };
    ps5_agc_register pixel_sh[6] = {
        {6, 0}, {6, 0}, {8, 3}, {9, 4}, {0xa, 0x002c0000}, {0xb, 0},
    };
    ps5_agc_register pre_cx[PS5_PIPELINE_PRE_RASTER_CX_REGISTERS];
    ps5_agc_register pixel_cx[PS5_PIPELINE_PIXEL_CX_REGISTERS];
    struct ps5_agc_linked_cx linked_cx;
    struct ps5_agc_linked_uc linked_uc;
    struct ps5_pipeline_registers a, b;
    memset(&linked_cx, 0x12, sizeof(linked_cx));
    memset(&linked_uc, 0x34, sizeof(linked_uc));
    for (uint32_t i = 0; i < PS5_PIPELINE_PRE_RASTER_CX_REGISTERS; ++i)
        pre_cx[i] = (ps5_agc_register){0x200u + i, 0x5000u + i};
    for (uint32_t i = 0; i < PS5_PIPELINE_PIXEL_CX_REGISTERS; ++i)
        pixel_cx[i] = (ps5_agc_register){0x300u + i, 0x6000u + i};
    for (uint32_t i = 0; i < 16u; ++i)
        rt[i] = (ps5_agc_register){offsets[i], i};
    assert(ps5_pipeline_build(&a, rt, &linked_cx, &linked_uc,
                              pre_cx, pixel_cx, pre_sh, pixel_sh,
                              1920u, 1080u) == 0);
    assert(ps5_pipeline_build(&b, rt, &linked_cx, &linked_uc,
                              pre_cx, pixel_cx, pre_sh, pixel_sh,
                              1920u, 1080u) == 0);
    assert(memcmp(&a, &b, sizeof(a)) == 0);
    assert(fnv1a64(&a, sizeof(a)) == UINT64_C(0x46d786bedc9961b3));
    assert(a.cx[0].offset == 0x318u && a.cx[15].offset == 0x3b8u);
    assert(a.cx[16].offset == 0x10fu && a.cx[30].offset == 0x08eu);
    assert(memcmp(&a.cx[31], &linked_cx, sizeof(linked_cx)) == 0);
    assert(memcmp(&a.cx[65], pre_cx, sizeof(pre_cx)) == 0);
    assert(memcmp(&a.cx[75], pixel_cx, sizeof(pixel_cx)) == 0);
    assert(memcmp(a.sh, pre_sh, sizeof(pre_sh)) == 0);
    assert(memcmp(a.sh + 6, pixel_sh, sizeof(pixel_sh)) == 0);
    assert(memcmp(a.uc, &linked_uc, sizeof(linked_uc)) == 0);
    rt[3].offset++;
    assert(ps5_pipeline_build(&a, rt, &linked_cx, &linked_uc,
                              pre_cx, pixel_cx, pre_sh, pixel_sh,
                              1920u, 1080u) == -2);
    return 0;
}
