#ifndef PS5_AGC_GEARS_MESH_H
#define PS5_AGC_GEARS_MESH_H

#include <stddef.h>
#include <stdint.h>

typedef struct GearsVertex {
    float position[3];
    float normal[3];
} GearsVertex;

typedef struct GearSpec {
    float inner_radius;
    float root_radius;
    float tip_radius;
    float thickness;
    uint32_t teeth;
} GearSpec;

/* Mesa es2gears topology expanded from seven strips to 20 triangles/tooth. */
size_t gears_mesh_vertex_count(uint32_t teeth);

/* Returns the emitted vertex count, or zero for invalid input/capacity. */
size_t gears_build_mesh(GearsVertex *out, size_t capacity,
                        const GearSpec *spec);

#endif
