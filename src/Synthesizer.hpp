#pragma once

#include <stdint.h>
#include <zephyr/sys_clock.h>

#include "KeyPress.hpp"
#include "Synthesizer/Oscillator.hpp"

class Synthesizer {
   public:
    typedef enum {
        OSC1,
        OSC2,
        MASTER,

        COUNT,
    } Mode;

    typedef enum {
        LFO_MOD,
        AMP_MOD,
        SPECIAL,
    } Effect;

   private:
    static uint8_t master_volume;
    static Oscillator osc[2];
    static Mode current_mode;
    static Effect current_effect;

   public:
    // Disallow creating an instance of this class.
    Synthesizer() = delete;

    /// @brief Synthesizer initialization function
    /// Call this function during initialization before calling the rest of the fuctions
    static void init(void);

    static void set_mode(Mode oscillator);
    static void set_effect(Effect effect);

    static void change_waveform(bool must_increase);
    static void change_pitch(bool must_increase);
    static void change_volume(bool must_increase);

    /// @brief Populate the audio buffer with sound
    /// @param block the audio block
    /// @param timeout timeout for the operation.
    /// @return 0 on success, otherwise ERRNO
    static int synthesize(int16_t *block, k_timeout_t timeout);

    /// @brief Compute next sound value for a specific key
    /// @param key the key you want to generate sound with
    /// @return the next sound value
    static int16_t compute_sample(KeyPress &key);
};
