#include "bsp_sky.h"

#include <float.h>
#include <math.h>
#include <string.h>

int bsp_sky_plan(const BspBundleView *bundle,
                 const float camera_position[3],
                 BspSkyPlan *out)
{
    if (!bundle || !camera_position || !out || !bundle->textures ||
        bundle->texture_count == 0u || !bundle->draws ||
        bundle->draw_count == 0u || !bundle->indices ||
        bundle->index_count == 0u || !bundle->vertices ||
        bundle->vertex_count == 0u)
        return -1;
    memset(out, 0, sizeof(*out));
    out->target_texture = UINT32_MAX;
    out->target_face = UINT32_MAX;
    for (uint32_t texture = 0u; texture < bundle->texture_count; ++texture)
        if ((bundle->textures[texture].flags & BSP_BUNDLE_TEXTURE_SKY) != 0u)
            ++out->texture_count;
    if (out->texture_count == 0u)
        return -2;

    float closest = FLT_MAX;
    for (uint32_t index = 0u; index < bundle->draw_count; ++index) {
        const BspBundleDraw *const draw = &bundle->draws[index];
        if (draw->base_texture >= bundle->texture_count ||
            draw->first_index > bundle->index_count ||
            draw->index_count == 0u || draw->index_count % 3u != 0u ||
            draw->index_count > bundle->index_count - draw->first_index)
            return -3;
        if ((bundle->textures[draw->base_texture].flags &
             BSP_BUNDLE_TEXTURE_SKY) == 0u)
            continue;
        ++out->draw_count;
        float center[3] = {0.0f, 0.0f, 0.0f};
        for (uint32_t element = 0u; element < draw->index_count; ++element) {
            const uint32_t vertex =
                bundle->indices[draw->first_index + element];
            if (vertex >= bundle->vertex_count)
                return -3;
            for (unsigned axis = 0u; axis < 3u; ++axis)
                center[axis] += bundle->vertices[vertex].position[axis];
        }
        for (unsigned axis = 0u; axis < 3u; ++axis)
            center[axis] /= (float)draw->index_count;
        const float delta[3] = {
            center[0] - camera_position[0],
            center[1] - camera_position[1],
            center[2] - camera_position[2],
        };
        const float distance = delta[0] * delta[0] +
                               delta[1] * delta[1] +
                               delta[2] * delta[2];
        if (!(distance > 0.000001f) || !__builtin_isfinite(distance))
            continue;
        if (distance < closest) {
            closest = distance;
            out->target_texture = draw->base_texture;
            out->target_face = draw->face_id;
            const float inverse = 1.0f / sqrtf(distance);
            const float travel = sqrtf(distance) > 64.0f
                ? sqrtf(distance) - 64.0f : 0.0f;
            for (unsigned axis = 0u; axis < 3u; ++axis) {
                out->target_forward[axis] = delta[axis] * inverse;
                out->target_position[axis] = camera_position[axis] +
                    out->target_forward[axis] * travel;
            }
        }
    }
    if (out->draw_count == 0u || out->target_texture == UINT32_MAX ||
        out->target_face == UINT32_MAX)
        return -4;
    return 0;
}
