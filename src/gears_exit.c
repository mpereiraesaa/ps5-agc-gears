#include "gears_exit.h"

#include <string.h>

void gears_exit_init(GearsExitControl *control)
{
    if (control)
        memset(control, 0, sizeof(*control));
}

int gears_exit_update(GearsExitControl *control, uint32_t buttons,
                      int input_usable, uint32_t exit_button)
{
    if (!control || exit_button == 0u)
        return 0;
    const uint32_t pressed = buttons & ~control->previous_buttons;
    control->previous_buttons = buttons;
    if (input_usable && (pressed & exit_button) != 0u)
        control->requested = 1;
    return control->requested;
}
