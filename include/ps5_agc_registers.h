#ifndef PS5_AGC_GEARS_REGISTERS_H
#define PS5_AGC_GEARS_REGISTERS_H

#include <stddef.h>
#include <stdint.h>

#include "ps5_agc.h"

struct ps5_agc_linked_cx {
    ps5_agc_register spi_ps_input_cntl[32];
    ps5_agc_register vgt_shader_stages_en;
    ps5_agc_register vgt_gs_out_prim_type;
};

struct ps5_agc_linked_uc {
    ps5_agc_register ge_cntl;
    ps5_agc_register ge_user_vgpr_en;
    ps5_agc_register vgt_primitive_type;
};

struct ps5_agc_type_index {
    uint32_t key;
    uint32_t encoded_index;
};

struct ps5_agc_register_defaults {
    ps5_agc_register **table_cx;
    ps5_agc_register **table_sh;
    ps5_agc_register **table_uc;
    ps5_agc_register **table_3;
    uint64_t unknown_20;
    uint64_t unknown_28;
    struct ps5_agc_type_index *type_index_pairs;
    uint32_t count;
    uint32_t reserved_3c;
};

static inline uint32_t ps5_agc_type_index_bank(
    const struct ps5_agc_type_index *record)
{
    return record->encoded_index & 3u;
}

static inline uint32_t ps5_agc_type_index_value(
    const struct ps5_agc_type_index *record)
{
    return record->encoded_index >> 2;
}

#ifdef __cplusplus
#define PS5_AGC_STATIC_ASSERT static_assert
#else
#define PS5_AGC_STATIC_ASSERT _Static_assert
#endif

PS5_AGC_STATIC_ASSERT(sizeof(struct ps5_agc_linked_cx) == 0x110,
               "FW 12.02 linked CX ABI");
PS5_AGC_STATIC_ASSERT(sizeof(struct ps5_agc_linked_uc) == 0x18,
               "FW 12.02 linked UC ABI");
PS5_AGC_STATIC_ASSERT(sizeof(struct ps5_agc_register_defaults) == 0x40,
               "FW 12.02 defaults root ABI");
PS5_AGC_STATIC_ASSERT(offsetof(struct ps5_agc_register_defaults, type_index_pairs) ==
                   0x30,
               "FW 12.02 defaults type table ABI");

#undef PS5_AGC_STATIC_ASSERT

#endif
