#ifndef PS5_AGC_GEARS_SCENE_H
#define PS5_AGC_GEARS_SCENE_H

#include <stdint.h>

enum { GEARS_SCENE_DRAW_COUNT = 3, GEARS_SCENE_SH_WORDS = 25 };

typedef struct GearsSceneDraw {
    uint32_t sh_words[GEARS_SCENE_SH_WORDS];
    uint32_t vertex_count;
} GearsSceneDraw;

/* Clean-room 20:10:10 external-gear train used by the demo. Positions are
 * center coordinates; angles are radians. */
void gears_scene_pose(float time, float positions[GEARS_SCENE_DRAW_COUNT][3],
                      float angles[GEARS_SCENE_DRAW_COUNT]);

/* Builds shader-direct MVP[16], quaternion[4], material[4], SRD pointer[1]. */
int gears_build_scene(GearsSceneDraw out[GEARS_SCENE_DRAW_COUNT], float time,
                      const uint32_t srd_table_addresses[GEARS_SCENE_DRAW_COUNT],
                      const uint32_t vertex_counts[GEARS_SCENE_DRAW_COUNT]);

#endif
