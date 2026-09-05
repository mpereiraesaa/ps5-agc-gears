#include "../src/bsp_noclip.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>

static int close_enough(float left, float right)
{
    return fabsf(left - right) < 0.0005f;
}

int main(void)
{
    const float origin[3] = {10.0f, 20.0f, 30.0f};
    const float initial_forward[3] = {-2.0f, 0.0f, 0.0f};
    BspNoclipCamera camera;
    assert(bsp_noclip_init(&camera, origin, initial_forward) == 0);
    assert(close_enough(camera.forward[0], -1.0f));
    assert(close_enough(camera.forward[1], 0.0f));
    assert(close_enough(camera.forward[2], 0.0f));

    BspNoclipInput neutral = {128, 128, 128, 128, 0, 0, 1};
    assert(bsp_noclip_step(&camera, &neutral, 1.0f / 60.0f) == 0);
    assert(camera.sampled_frames == 1u && camera.connected_frames == 1u);
    assert(camera.moving_frames == 0u && camera.looking_frames == 0u);
    for (unsigned component = 0; component < 3u; ++component)
        assert(close_enough(camera.position[component], origin[component]));

    BspNoclipInput advance = neutral;
    advance.left_y = 0u;
    assert(bsp_noclip_step(&camera, &advance, 1.0f / 60.0f) == 0);
    assert(camera.moving_frames == 1u && camera.input_changes == 1u);
    assert(camera.position[0] < origin[0]);
    assert(close_enough(camera.distance_travelled, 320.0f / 60.0f));

    BspNoclipCamera strafe_camera;
    assert(bsp_noclip_init(&strafe_camera, origin, initial_forward) == 0);
    BspNoclipInput strafe = neutral;
    strafe.left_x = 255u;
    assert(bsp_noclip_step(&strafe_camera, &strafe, 1.0f / 60.0f) == 0);
    assert(strafe_camera.position[2] < origin[2]);

    BspNoclipInput look = neutral;
    look.right_x = 255u;
    look.right_y = 0u;
    assert(bsp_noclip_step(&camera, &look, 1.0f / 60.0f) == 0);
    assert(camera.looking_frames == 1u);
    assert(camera.forward[1] > 0.0f && camera.forward[2] < 0.0f);

    BspNoclipInput rise = neutral;
    rise.r2 = 255u;
    const float before_y = camera.position[1];
    assert(bsp_noclip_step(&camera, &rise, 1.0f) == 0);
    assert(camera.position[1] > before_y);
    assert(camera.distance_travelled < 32.0f);

    BspNoclipInput disconnected = neutral;
    disconnected.connected = 0;
    const float before[3] = {camera.position[0], camera.position[1],
                             camera.position[2]};
    assert(bsp_noclip_step(&camera, &disconnected, 1.0f / 60.0f) == 0);
    assert(camera.sampled_frames == 5u && camera.connected_frames == 4u);
    for (unsigned component = 0; component < 3u; ++component)
        assert(close_enough(camera.position[component], before[component]));

    BspNoclipCamera soak;
    assert(bsp_noclip_init(&soak, origin, initial_forward) == 0);
    BspNoclipInput moving = neutral;
    moving.left_y = 32u;
    moving.right_x = 176u;
    for (unsigned frame = 0; frame < 10000u; ++frame) {
        moving.left_x = (frame & 127u) < 64u ? 96u : 160u;
        assert(bsp_noclip_step(&soak, &moving, 1.0f / 60.0f) == 0);
    }
    assert(soak.sampled_frames == 10000u);
    assert(soak.connected_frames == 10000u);
    assert(soak.moving_frames == 10000u);
    assert(soak.looking_frames == 10000u);
    assert(soak.input_changes > 100u);
    assert(soak.distance_travelled > 35000.0f);
    assert(soak.input_hash != UINT64_C(14695981039346656037));

    assert(bsp_noclip_init(0, origin, initial_forward) != 0);
    const float zero[3] = {0.0f, 0.0f, 0.0f};
    assert(bsp_noclip_init(&camera, origin, zero) != 0);
    assert(bsp_noclip_step(&camera, &neutral, 0.0f) != 0);
    return 0;
}
