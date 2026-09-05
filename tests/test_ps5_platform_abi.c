#include "../include/ps5_platform.h"

#include <assert.h>

int main(void)
{
    assert(offsetof(struct ps5_batch_map_entry, protection) == 0x18u);
    assert(offsetof(struct ps5_batch_map_entry, memory_type) == 0x19u);
    assert(sizeof(struct ps5_video_attribute) == 80u);
    return 0;
}
