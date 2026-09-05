#include "../src/bsp_flat_scene.h"

#include <assert.h>
#include <math.h>
#include <string.h>

static float value(uint32_t bits)
{
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

int main(void)
{
    const uint32_t indices[6] = {0, 1, 2, 2, 3, 0};
    const BspBundleDraw source[2] = {
        {.first_index = 0, .index_count = 3, .face_id = 7},
        {.first_index = 3, .index_count = 3, .face_id = 8},
    };
    BspBundleView bundle = {0};
    bundle.indices = indices;
    bundle.index_count = 6;
    bundle.draws = source;
    bundle.draw_count = 2;
    bundle.camera_position[0] = 32.0f;
    bundle.camera_position[1] = 36.0f;
    bundle.camera_position[2] = -16.0f;
    bundle.camera_forward[2] = -1.0f;
    BspFlatDraw draws[2];
    assert(bsp_flat_build_scene(draws, 2u, &bundle, 0x12340000u,
                                16.0f / 9.0f) == 0);
    assert(draws[0].indices == indices && draws[1].indices == indices + 3u);
    assert(draws[0].index_count == 3u && draws[1].index_count == 3u);
    assert(draws[0].sh_words[20] == 0x12340000u &&
           draws[1].sh_words[20] == 0x12340000u);
    for (unsigned word = 0; word < 16u; ++word) {
        assert(draws[0].sh_words[word] == draws[1].sh_words[word]);
        assert(isfinite(value(draws[0].sh_words[word])));
    }
    assert(draws[0].sh_words[16] != draws[1].sh_words[16]);
    for (unsigned word = 16u; word < 19u; ++word)
        assert(value(draws[0].sh_words[word]) >= 0.25f &&
               value(draws[0].sh_words[word]) <= 0.95f);
    assert(value(draws[0].sh_words[19]) == 1.0f);
    bundle.camera_forward[2] = 0.0f;
    assert(bsp_flat_build_scene(draws, 2u, &bundle, 0x12340000u,
                                16.0f / 9.0f) == -2);
    return 0;
}
