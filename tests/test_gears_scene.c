#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "../src/gears_scene.h"

static float value(uint32_t bits)
{
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

int main(void)
{
    const uint32_t tables[3] = {0x10000, 0x20000, 0x30000};
    const uint32_t counts[3] = {1200, 600, 600};
    GearsSceneDraw draws[3];
    assert(sizeof(draws[0].sh_words) == 25 * sizeof(uint32_t));
    assert(gears_build_scene(draws, 1.25f, tables, counts) == 0);
    for (unsigned draw = 0; draw < 3; ++draw) {
        float norm = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const float q = value(draws[draw].sh_words[16 + i]);
            norm += q * q;
        }
        assert(fabsf(norm - 1.0f) < 0.0001f);
        assert(draws[draw].sh_words[24] == tables[draw]);
        assert(draws[draw].vertex_count == counts[draw]);
        for (unsigned i = 0; i < 24; ++i)
            assert(isfinite(value(draws[draw].sh_words[i])));
    }
    assert(draws[0].sh_words[21] != draws[1].sh_words[21]);
    assert(fabsf(value(draws[0].sh_words[20]) - 0.80f) < 0.0001f);
    assert(fabsf(value(draws[0].sh_words[21]) - 0.10f) < 0.0001f);
    assert(fabsf(value(draws[1].sh_words[20]) - 0.00f) < 0.0001f);
    assert(fabsf(value(draws[1].sh_words[21]) - 0.80f) < 0.0001f);
    assert(fabsf(value(draws[2].sh_words[22]) - 1.00f) < 0.0001f);

    float p0[3][3], p1[3][3], a0[3], a1[3];
    gears_scene_pose(0.0f, p0, a0);
    gears_scene_pose(0.75f, p1, a1);
    const float expected_distance[2] = {1.525f, 1.5502016f};
    for (unsigned i = 0; i < 2; ++i) {
        const float dx = p0[i + 1][0] - p0[0][0];
        const float dy = p0[i + 1][1] - p0[0][1];
        assert(fabsf(sqrtf(dx * dx + dy * dy) - expected_distance[i]) < 0.0001f);
        assert(fabsf((a1[i + 1] - a0[i + 1]) +
                     2.0f * (a1[0] - a0[0])) < 0.0001f);
        assert(fabsf(p1[i + 1][0] - p0[i + 1][0]) < 0.0001f);
        assert(fabsf(p1[i + 1][1] - p0[i + 1][1]) < 0.0001f);
    }
    assert(fabsf(a0[0]) < 0.0001f);
    assert(fabsf(a0[1] + 0.1570796327f) < 0.0001f);
    assert(fabsf(a0[2] + 0.4363323130f) < 0.0001f);
    assert(gears_build_scene(draws, NAN, tables, counts) == -1);
    return 0;
}
