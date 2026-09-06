#ifndef PS5_AGC_GEARS_BSP_NOCLIP_H
#define PS5_AGC_GEARS_BSP_NOCLIP_H

#include <stdint.h>

enum {
    BSP_NOCLIP_STICK_DEADZONE = 12,
};

typedef struct BspNoclipInput {
    uint8_t left_x;
    uint8_t left_y;
    uint8_t right_x;
    uint8_t right_y;
    uint8_t l2;
    uint8_t r2;
    int connected;
} BspNoclipInput;

typedef struct BspNoclipCamera {
    float position[3];
    float forward[3];
    float yaw;
    float pitch;
    float distance_travelled;
    uint64_t sampled_frames;
    uint64_t connected_frames;
    uint64_t moving_frames;
    uint64_t looking_frames;
    uint64_t input_changes;
    uint64_t input_hash;
    uint64_t previous_input;
    int has_previous_input;
} BspNoclipCamera;

int bsp_noclip_init(BspNoclipCamera *camera, const float position[3],
                    const float forward[3]);
int bsp_noclip_step(BspNoclipCamera *camera, const BspNoclipInput *input,
                    float delta_seconds);

#endif
