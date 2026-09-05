#ifndef PS5_AGC_GEARS_SHADER_H
#define PS5_AGC_GEARS_SHADER_H

#include "ps5_agc.h"
#include <stddef.h>

struct ps5_shader_user_data {
    uint16_t *direct_resource_offsets;
    void *sharp_resource_offsets[4];
    uint16_t eud_size_dw;
    uint16_t srt_size_dw;
    uint16_t direct_resource_count;
    uint16_t sharp_resource_count[4];
};

struct ps5_shader_header {
    uint32_t file_header;
    uint32_t version;
    struct ps5_shader_user_data *user_data;
    const void *code;
    ps5_agc_register *cx_registers;
    ps5_agc_register *sh_registers;
    void *specials;
    void *input_semantics;
    void *output_semantics;
    uint32_t header_size;
    uint32_t shader_size;
    uint32_t embedded_constant_dqw;
    uint32_t target;
    uint32_t num_input_semantics;
    uint16_t scratch_dw_per_thread;
    uint16_t num_output_semantics;
    uint16_t special_sizes_bytes;
    uint8_t type;
    uint8_t num_cx_registers;
    uint8_t num_sh_registers;
    uint8_t reserved_5d[3];
};

#ifdef __cplusplus
#define PS5_SHADER_STATIC_ASSERT static_assert
#else
#define PS5_SHADER_STATIC_ASSERT _Static_assert
#endif
PS5_SHADER_STATIC_ASSERT(sizeof(struct ps5_shader_user_data) == 0x38,
                         "shader user-data ABI");
PS5_SHADER_STATIC_ASSERT(sizeof(struct ps5_shader_header) == 0x60,
                         "shader header ABI");
PS5_SHADER_STATIC_ASSERT(offsetof(struct ps5_shader_header, code) == 0x10,
                         "shader code ABI");
PS5_SHADER_STATIC_ASSERT(offsetof(struct ps5_shader_header, type) == 0x5a,
                         "shader type ABI");
#undef PS5_SHADER_STATIC_ASSERT

#endif
