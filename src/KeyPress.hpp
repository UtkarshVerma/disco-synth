#pragma once

#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/sys_clock.h>

#include "Synthesizer/Key.hpp"

/// @brief Maximum number of keys. Space allocated at compile time.
constexpr uint8_t MAX_KEYPRESSES = 4;

class KeyPress {
   public:
    Key k;
    enum { IDLE, PRESSED, RELEASED } state;
    k_timepoint_t hold_time;
    k_timepoint_t release_time;
    uint16_t phase[2];

    KeyPress(void);

    /// @brief Get the frequency of this specific key
    /// @return this key's frequency
    float get_freq(void);
};

extern KeyPress keypresses[MAX_KEYPRESSES];
