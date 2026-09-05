#include <assert.h>
#include <string.h>

#include "../src/gears_rt_clear.h"

static int set_sh(uint32_t **cursor, uint32_t capacity, uint32_t offset,
                  const uint32_t *values, uint32_t count)
{
    if (capacity < 27) return -1;
    assert(offset == 0x8d && values && count == 25);
    *cursor += 27;
    return 0;
}

static int draw(uint32_t **cursor, uint32_t capacity, uint32_t count,
                uint64_t modifier)
{
    if (capacity < 3) return -1;
    assert(count == 3 && modifier == 5);
    *cursor += 3;
    return 0;
}

int main(void)
{
    GearsRtClearVertex vertices[3] = {0};
    GearsSceneDraw clear;
    const float black[4] = {0, 0, 0, 1};
    assert(gears_rt_clear_build(vertices, &clear, 0x12340000, black) == 0);
    assert(vertices[0].position[0] == -1.0f &&
           vertices[1].position[0] == 3.0f &&
           vertices[2].position[1] == 3.0f);
    assert(clear.vertex_count == 3 && clear.sh_words[24] == 0x12340000);
    uint32_t words[30] = {0};
    uint32_t *cursor = words;
    GearsDrawComposeResult result;
    assert(gears_rt_clear_compose(&cursor, words + 30, &clear, 5,
                                  set_sh, draw, &result) == 0);
    assert(cursor == words + 30 && result.draws == 1 &&
           result.command_dwords == 30);
    cursor = words;
    assert(gears_rt_clear_compose(&cursor, words + 29, &clear, 5,
                                  set_sh, draw, &result) != 0);
    return 0;
}
