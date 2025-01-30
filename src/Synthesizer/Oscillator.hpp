#pragma once

#include <cstddef>
#include <cstdint>

class Oscillator {
   public:
    typedef enum {
        SINE,
        TRIANGLE,
        SQUARE,
        SAWTOOTH,

        COUNT,
    } WaveType;

   private:
    WaveType wave;
    uint8_t volume;
    size_t freq_shift_index;

   public:
    Oscillator(void);

    float get_freq_shift(void);

    /// @brief Compute the oscillator output sample
    /// @param phase the current phase to generate sample with
    /// @return the next oscillator sample
    int16_t compute_sample(uint16_t phase);

    WaveType change_waveform(bool must_increase);
    void change_pitch(bool must_increase);
    uint16_t change_volume(bool must_increase);
};
