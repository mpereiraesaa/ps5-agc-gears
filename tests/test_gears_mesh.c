#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "../src/gears_mesh.h"

int main(void)
{
    const GearSpec spec = {0.45f, 1.00f, 1.20f, 0.35f, 20};
    const size_t count = gears_mesh_vertex_count(spec.teeth);
    assert(sizeof(GearsVertex) == 24);
    assert(count == 1200);
    GearsVertex *vertices = calloc(count, sizeof(*vertices));
    assert(vertices);
    assert(gears_build_mesh(vertices, count, &spec) == count);
    for (size_t i = 0; i < count; ++i) {
        const float r = hypotf(vertices[i].position[0], vertices[i].position[1]);
        const float n = sqrtf(vertices[i].normal[0] * vertices[i].normal[0] +
                              vertices[i].normal[1] * vertices[i].normal[1] +
                              vertices[i].normal[2] * vertices[i].normal[2]);
        assert(r >= spec.inner_radius - 0.0001f);
        assert(r <= spec.tip_radius + 0.0001f);
        assert(fabsf(vertices[i].position[2]) == spec.thickness * 0.5f);
        assert(fabsf(n - 1.0f) < 0.0001f);
    }
    assert(gears_build_mesh(vertices, count - 1, &spec) == 0);
    assert(gears_mesh_vertex_count(2) == 0);
    free(vertices);
    return 0;
}
