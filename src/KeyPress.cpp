#include "KeyPress.hpp"

#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>

#include "Synthesizer/Key.hpp"

KeyPress keypresses[MAX_KEYPRESSES];

KeyPress::KeyPress(void)
    : k{Key::A3},
      state{IDLE},
      hold_time{sys_timepoint_calc(K_FOREVER)},
      release_time{sys_timepoint_calc(K_FOREVER)},
      phase{0, 0} {}
