#include "bsp_noclip.h"

#include <math.h>
#include <string.h>

enum {
    BSP_NOCLIP_TRIGGER_DEADZONE = 8,
};

static const float move_speed = 320.0f;
static const float look_speed = 2.4f;
static const float pitch_limit = 1.55334306f;
static const float max_delta_seconds = 0.05f;

static float centered_axis(uint8_t value)
{
    const int delta = (int)value - 128;
    const int magnitude = delta < 0 ? -delta : delta;
    if (magnitude <= BSP_NOCLIP_STICK_DEADZONE)
        return 0.0f;
    const int span = delta < 0 ? 128 - BSP_NOCLIP_STICK_DEADZONE
                               : 127 - BSP_NOCLIP_STICK_DEADZONE;
    const float scaled = (float)(magnitude - BSP_NOCLIP_STICK_DEADZONE) /
                         (float)span;
    return delta < 0 ? -scaled : scaled;
}

static float trigger_axis(uint8_t value)
{
    if (value <= BSP_NOCLIP_TRIGGER_DEADZONE)
        return 0.0f;
    return (float)(value - BSP_NOCLIP_TRIGGER_DEADZONE) /
           (float)(255 - BSP_NOCLIP_TRIGGER_DEADZONE);
}

static uint64_t packed_input(const BspNoclipInput *input)
{
    return (uint64_t)input->left_x |
        (uint64_t)input->left_y << 8u |
        (uint64_t)input->right_x << 16u |
        (uint64_t)input->right_y << 24u |
        (uint64_t)input->l2 << 32u |
        (uint64_t)input->r2 << 40u |
        (uint64_t)(input->connected != 0) << 48u;
}

static void update_hash(BspNoclipCamera *camera, uint64_t sample)
{
    for (unsigned byte = 0; byte < 7u; ++byte) {
        camera->input_hash ^= (sample >> (byte * 8u)) & UINT64_C(0xff);
        camera->input_hash *= UINT64_C(1099511628211);
    }
}

static void update_forward(BspNoclipCamera *camera)
{
    const float horizontal = cosf(camera->pitch);
    camera->forward[0] = horizontal * cosf(camera->yaw);
    camera->forward[1] = sinf(camera->pitch);
    camera->forward[2] = horizontal * sinf(camera->yaw);
}

int bsp_noclip_init(BspNoclipCamera *camera, const float position[3],
                    const float forward[3])
{
    if (!camera || !position || !forward)
        return -1;
    const float length = sqrtf(forward[0] * forward[0] +
                               forward[1] * forward[1] +
                               forward[2] * forward[2]);
    if (!(length > 0.000001f) || !__builtin_isfinite(length))
        return -1;
    memset(camera, 0, sizeof(*camera));
    for (unsigned component = 0; component < 3u; ++component) {
        if (!__builtin_isfinite(position[component]) ||
            !__builtin_isfinite(forward[component]))
            return -1;
        camera->position[component] = position[component];
    }
    float normalized_y = forward[1] / length;
    if (normalized_y < -1.0f) normalized_y = -1.0f;
    if (normalized_y > 1.0f) normalized_y = 1.0f;
    camera->yaw = atan2f(forward[2], forward[0]);
    camera->pitch = asinf(normalized_y);
    camera->input_hash = UINT64_C(14695981039346656037);
    update_forward(camera);
    return 0;
}

int bsp_noclip_step(BspNoclipCamera *camera, const BspNoclipInput *input,
                    float delta_seconds)
{
    if (!camera || !input || !(delta_seconds > 0.0f) ||
        !__builtin_isfinite(delta_seconds))
        return -1;
    if (delta_seconds > max_delta_seconds)
        delta_seconds = max_delta_seconds;
    ++camera->sampled_frames;
    const uint64_t sample = packed_input(input);
    update_hash(camera, sample);
    if (camera->has_previous_input && sample != camera->previous_input)
        ++camera->input_changes;
    camera->previous_input = sample;
    camera->has_previous_input = 1;
    if (!input->connected)
        return 0;
    ++camera->connected_frames;

    const float look_x = centered_axis(input->right_x);
    const float look_y = centered_axis(input->right_y);
    if (look_x != 0.0f || look_y != 0.0f) {
        camera->yaw += look_x * look_speed * delta_seconds;
        camera->pitch -= look_y * look_speed * delta_seconds;
        if (camera->pitch < -pitch_limit) camera->pitch = -pitch_limit;
        if (camera->pitch > pitch_limit) camera->pitch = pitch_limit;
        ++camera->looking_frames;
    }
    update_forward(camera);

    const float strafe = centered_axis(input->left_x);
    const float advance = -centered_axis(input->left_y);
    const float vertical = trigger_axis(input->r2) - trigger_axis(input->l2);
    if (strafe == 0.0f && advance == 0.0f && vertical == 0.0f)
        return 0;

    const float right_x = -camera->forward[2];
    const float right_z = camera->forward[0];
    float motion[3] = {
        camera->forward[0] * advance + right_x * strafe,
        camera->forward[1] * advance + vertical,
        camera->forward[2] * advance + right_z * strafe,
    };
    float magnitude = sqrtf(motion[0] * motion[0] +
                            motion[1] * motion[1] +
                            motion[2] * motion[2]);
    if (!(magnitude > 0.0f) || !__builtin_isfinite(magnitude))
        return -1;
    if (magnitude > 1.0f) {
        motion[0] /= magnitude;
        motion[1] /= magnitude;
        motion[2] /= magnitude;
        magnitude = 1.0f;
    }
    const float distance = move_speed * delta_seconds;
    camera->position[0] += motion[0] * distance;
    camera->position[1] += motion[1] * distance;
    camera->position[2] += motion[2] * distance;
    camera->distance_travelled += magnitude * distance;
    ++camera->moving_frames;
    return __builtin_isfinite(camera->position[0]) &&
           __builtin_isfinite(camera->position[1]) &&
           __builtin_isfinite(camera->position[2]) &&
           __builtin_isfinite(camera->distance_travelled) ? 0 : -1;
}
