/*
 * Triangle-list adaptation of Mesa demos' es2gears gear generator.
 * Original: Copyright (C) 1999-2001 Brian Paul, SPDX-License-Identifier: MIT
 * GLES2 refactor: Kristian Høgsberg and Alexandros Frantzis.
 * Upstream revision: 649baedafcb90313ade69909fdef1ee156ab5f8d
 */
#include "gears_mesh.h"

#include <math.h>

enum { GEAR_VERTICES_PER_TOOTH = 60 };
static const float kTau = 6.28318530717958647692f;

size_t gears_mesh_vertex_count(uint32_t teeth)
{
    if (teeth < 3 || teeth > 256) return 0;
    return (size_t)teeth * GEAR_VERTICES_PER_TOOTH;
}

static GearsVertex vertex(float x, float y, float z,
                          float nx, float ny, float nz)
{
    const float length = sqrtf(nx * nx + ny * ny + nz * nz);
    if (length > 0.0f) { nx /= length; ny /= length; nz /= length; }
    GearsVertex result = {{x, y, z}, {nx, ny, nz}};
    return result;
}

static void triangle(GearsVertex **out, GearsVertex a, GearsVertex b,
                     GearsVertex c)
{
    *(*out)++ = a; *(*out)++ = b; *(*out)++ = c;
}

static void strip(GearsVertex **out, const GearsVertex *vertices, size_t count)
{
    for (size_t i = 2; i < count; ++i) {
        if (i & 1u)
            triangle(out, vertices[i - 1], vertices[i - 2], vertices[i]);
        else
            triangle(out, vertices[i - 2], vertices[i - 1], vertices[i]);
    }
}

size_t gears_build_mesh(GearsVertex *out, size_t capacity,
                        const GearSpec *spec)
{
    if (!out || !spec || spec->inner_radius <= 0.0f ||
        spec->root_radius <= spec->inner_radius ||
        spec->tip_radius <= spec->root_radius || spec->thickness <= 0.0f)
        return 0;
    const size_t required = gears_mesh_vertex_count(spec->teeth);
    if (!required || capacity < required) return 0;

    const float half_z = spec->thickness * 0.5f;
    const float da = kTau / (float)spec->teeth / 4.0f;
    GearsVertex *cursor = out;
    for (uint32_t tooth = 0; tooth < spec->teeth; ++tooth) {
        float s[5], c[5];
        for (unsigned i = 0; i < 5; ++i) {
            const float angle = (float)tooth * kTau / (float)spec->teeth + da * (float)i;
            s[i] = sinf(angle); c[i] = cosf(angle);
        }
        const float px[7] = {
            spec->tip_radius*c[1], spec->tip_radius*c[2], spec->root_radius*c[0],
            spec->root_radius*c[3], spec->inner_radius*c[0], spec->root_radius*c[4],
            spec->inner_radius*c[4],
        };
        const float py[7] = {
            spec->tip_radius*s[1], spec->tip_radius*s[2], spec->root_radius*s[0],
            spec->root_radius*s[3], spec->inner_radius*s[0], spec->root_radius*s[4],
            spec->inner_radius*s[4],
        };
        GearsVertex face[7];
        for (unsigned i = 0; i < 7; ++i) face[i] = vertex(px[i], py[i], half_z, 0, 0, 1);
        strip(&cursor, face, 7);
        for (unsigned i = 0; i < 7; ++i) face[i] = vertex(px[i], py[i], -half_z, 0, 0, -1);
        strip(&cursor, face, 7);

        static const unsigned edges[4][2] = {{0,2}, {1,0}, {3,1}, {5,3}};
        for (unsigned edge = 0; edge < 4; ++edge) {
            const unsigned a = edges[edge][0], b = edges[edge][1];
            const float nx = py[a] - py[b], ny = -(px[a] - px[b]);
            const GearsVertex side[4] = {
                vertex(px[a],py[a],-half_z,nx,ny,0), vertex(px[a],py[a],half_z,nx,ny,0),
                vertex(px[b],py[b],-half_z,nx,ny,0), vertex(px[b],py[b],half_z,nx,ny,0),
            };
            strip(&cursor, side, 4);
        }
        const GearsVertex inner[4] = {
            vertex(px[4],py[4],-half_z,-c[0],-s[0],0), vertex(px[4],py[4],half_z,-c[0],-s[0],0),
            vertex(px[6],py[6],-half_z,-c[4],-s[4],0), vertex(px[6],py[6],half_z,-c[4],-s[4],0),
        };
        strip(&cursor, inner, 4);
    }
    return (size_t)(cursor - out);
}
