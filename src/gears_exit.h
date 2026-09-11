#ifndef GEARS_EXIT_H
#define GEARS_EXIT_H

#include <stdint.h>

typedef struct GearsExitControl {
    uint32_t previous_buttons;
    int requested;
} GearsExitControl;

void gears_exit_init(GearsExitControl *control);
int gears_exit_update(GearsExitControl *control, uint32_t buttons,
                      int input_usable, uint32_t exit_button);

#endif
