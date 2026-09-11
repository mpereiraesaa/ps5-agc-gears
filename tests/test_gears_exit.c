#include "../src/gears_exit.h"

#include <assert.h>

int main(void)
{
    const uint32_t options = 0x8u;
    GearsExitControl control;
    gears_exit_init(&control);
    assert(!gears_exit_update(&control, 0u, 1, options));
    assert(gears_exit_update(&control, options, 1, options));
    assert(gears_exit_update(&control, options, 1, options));

    gears_exit_init(&control);
    assert(!gears_exit_update(&control, options, 0, options));
    assert(!gears_exit_update(&control, options, 1, options));
    assert(!gears_exit_update(&control, 0u, 1, options));
    assert(gears_exit_update(&control, options, 1, options));
    assert(!gears_exit_update(0, options, 1, options));
    return 0;
}
