#include "Oscillator.hpp"

#include <sys/cdefs.h>
#include <zephyr/sys/util.h>

#include <cstddef>
#include <cstdint>

#include "sine.h"

constexpr auto MAX_VOLUME = 100;

static const float SHIFT_FREQUENCIES[] = {
    0.250, 0.265, 0.281, 0.297, 0.315, 0.334, 0.354, 0.375, 0.397, 0.420, 0.445, 0.472, 0.500,
    0.530, 0.561, 0.595, 0.630, 0.667, 0.707, 0.749, 0.794, 0.841, 0.891, 0.944, 1.000, 1.059,
    1.122, 1.189, 1.260, 1.335, 1.414, 1.498, 1.587, 1.682, 1.782, 1.888, 2.000, 2.119, 2.245,
    2.378, 2.520, 2.670, 2.828, 2.997, 3.175, 3.364, 3.564, 3.775, 4.000};

Oscillator::Oscillator(void) : wave(WaveType::SQUARE), volume(10), freq_shift_index(24) {}

float Oscillator::get_freq_shift(void) {
    return SHIFT_FREQUENCIES[this->freq_shift_index];
}

int16_t Oscillator::compute_sample(const uint16_t phase) {
    int16_t sample;

    switch (this->wave) {
        case SINE:
            sample = (int32_t)SINE_LUT[phase >> 6] - INT16_MIN;
            break;
        case SQUARE:
            sample = phase <= 0x8000 ? INT16_MIN : INT16_MAX;
            break;
        case TRIANGLE:
            sample = phase <= 0x8000 ? 2 * (phase - 0x4000)    // rising edge of triangle
                                     : -2 * (phase - 0xC000);  // falling edge of triangle
            break;
        case SAWTOOTH:
            sample = phase - 0x8000;
            break;

        default:
            __unreachable();
    }

    return (int32_t)sample * this->volume / MAX_VOLUME;
}

Oscillator::WaveType Oscillator::change_waveform(const bool must_increase) {
    const unsigned int delta = must_increase ? 1 : WaveType::COUNT - 1;
    this->wave = static_cast<WaveType>((static_cast<unsigned int>(this->wave) + delta) %
                                       WaveType::COUNT);

    return this->wave;
}

void Oscillator::change_pitch(const bool must_increase) {
    const int8_t delta = must_increase ? 1 : -1;
    this->freq_shift_index =
        CLAMP((int32_t)this->freq_shift_index + delta, 0, ARRAY_SIZE(SHIFT_FREQUENCIES) - 1);
}

uint16_t Oscillator::change_volume(const bool must_increase) {
    const int8_t delta = must_increase ? 2 : -2;
    this->volume       = CLAMP((int16_t)this->volume + delta, 0, MAX_VOLUME);

    return this->volume;
}
