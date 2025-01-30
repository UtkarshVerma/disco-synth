#include "Synthesizer.hpp"

#include <errno.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys_clock.h>

#include <cstdint>

#include "Audio.hpp"
#include "KeyPress.hpp"
#include "Synthesizer/Oscillator.hpp"
#include "USB.hpp"

LOG_MODULE_REGISTER(synthesizer, LOG_LEVEL_INF);

Oscillator Synthesizer::osc[];
Synthesizer::Mode Synthesizer::current_mode;
Synthesizer::Effect Synthesizer::current_effect;
uint8_t Synthesizer::master_volume;

static const char *const MODE_STRING_MAP[Synthesizer::Mode::COUNT] = {
    [Synthesizer::Mode::OSC1]   = "OSC1",
    [Synthesizer::Mode::OSC2]   = "OSC2",
    [Synthesizer::Mode::MASTER] = "MASTER",
};

static const char *const WAVETYPE_STRING_MAP[Oscillator::WaveType::COUNT] = {
    [Oscillator::WaveType::SINE]     = "Sine",
    [Oscillator::WaveType::TRIANGLE] = "Triangle",
    [Oscillator::WaveType::SQUARE]   = "Square",
    [Oscillator::WaveType::SAWTOOTH] = "Sawtooth",
};

void Synthesizer::init(void) {
    master_volume = UINT8_MAX;
}

void Synthesizer::change_waveform(const bool must_increase) {
    Oscillator::WaveType waveform;
    switch (current_mode) {
        case Mode::OSC1:
        case Mode::OSC2:
            waveform = osc[current_mode].change_waveform(must_increase);
            USB::println("[%s] Waveform: %s", MODE_STRING_MAP[current_mode],
                         WAVETYPE_STRING_MAP[waveform]);
            break;
        case Mode::MASTER:
            USB::println("[MASTER] LPF resonance (pending feature)");
            return;
        default:
            __unreachable();
    }
}

void Synthesizer::change_pitch(const bool must_increase) {
    switch (current_mode) {
        case Mode::OSC1:
        case Mode::OSC2:
            osc[current_mode].change_pitch(must_increase);
            USB::println("[%s] Pitch: %f Hz", MODE_STRING_MAP[current_mode],
                         osc[current_mode].get_freq_shift());
            break;
        case Mode::MASTER:
            USB::println("[MASTER] LPF cutoff frequency (pending feature)");
            break;
        default:
            __unreachable();
    }
}

void Synthesizer::change_volume(const bool must_increase) {
    uint16_t volume;
    int ret;
    switch (current_mode) {
        case Mode::OSC1:
        case Mode::OSC2:
            volume = osc[current_mode].change_volume(must_increase);
            break;
        case Mode::MASTER:
            master_volume = CLAMP(master_volume + (must_increase ? 1 : -1) * 2, 0, UINT8_MAX);
            volume        = master_volume;
            ret           = Audio::set_volume(master_volume);
            if (ret < 0) {
                LOG_ERR("Failed to set master volume: %d", -ret);
            }
            break;
        default:
            __unreachable();
    }

    USB::println("[%s] Volume: %u", MODE_STRING_MAP[current_mode], volume);
}

void Synthesizer::set_effect(const Effect effect) {
    current_effect = effect;

    switch (effect) {
        case LFO_MOD:
            USB::println("Configuring LFO modulator (not implemented)");
            break;
        case AMP_MOD:
            USB::println("Configuring amplitude modulator (not implemented)");
            break;
        case SPECIAL:
            USB::println("Configuring special effects (not implemented)");
            break;
    }
}

void Synthesizer::set_mode(const Mode mode) {
    // Save the previous state
    switch (mode) {
        case MASTER:
            // synth._master_volume_enc   = encoders[ENCODER_OSC_VOLUME].state;
            break;
        case OSC1:
            // synth._osc1.volume_enc     = encoders[ENCODER_OSC_VOLUME].state;
            // synth._osc1.wave_enc       = encoders[ENCODER_LPF_CUTOFF_OR_OSC_WAVE].state;
            // synth._osc1.freq_shift_enc = encoders[ENCODER_LPF_RES_OR_OSC_FREQ].state;
            break;
        case OSC2:
            // synth._osc2.volume_enc     = encoders[ENCODER_OSC_VOLUME].state;
            // synth._osc2.wave_enc       = encoders[ENCODER_LPF_CUTOFF_OR_OSC_WAVE].state;
            // synth._osc2.freq_shift_enc = encoders[ENCODER_LPF_RES_OR_OSC_FREQ].state;
            break;
        default:
            __unreachable();
    }

    // Load the new state
    switch (mode) {
        case MASTER:
            // encoders[ENCODER_OSC_VOLUME].state = synth._master_volume_enc;
            // encoders[LPF_CUTOFF_ENC].state     = synth._lpf._cutoff_freq;
            // encoders[LPF_RES_ENC].state        = synth._lpf._resonance_freq;
            break;
        case OSC1:
            // encoders[ENCODER_OSC_VOLUME].state             = synth._osc1.volume_enc;
            // encoders[ENCODER_LPF_CUTOFF_OR_OSC_WAVE].state = synth._osc1.wave_enc;
            // encoders[OSC_FREQ_ENC].state                   = synth._osc1.freq_shift_enc;
            break;
        case OSC2:
            // encoders[ENCODER_OSC_VOLUME].state             = synth._osc2.volume_enc;
            // encoders[ENCODER_LPF_CUTOFF_OR_OSC_WAVE].state = synth._osc2.wave_enc;
            // encoders[OSC_FREQ_ENC].state                   = synth._osc2.freq_shift_enc;
            break;
        default:
            __unreachable();
    }

    USB::println("[%s] Configured", MODE_STRING_MAP[mode]);

    current_mode = mode;

    // // Call the encoder's callbacks
    // encoders[ENCODER_OSC_VOLUME].callback(encoders[ENCODER_OSC_VOLUME]);
    // encoders[ENCODER_LPF_CUTOFF_OR_OSC_WAVE].callback(
    //     encoders[ENCODER_LPF_CUTOFF_OR_OSC_WAVE]);
    // encoders[OSC_FREQ_ENC].callback(encoders[OSC_FREQ_ENC]);
}

int16_t Synthesizer::compute_sample(KeyPress &key) {
    int16_t sample = 0;
    auto freq      = (float)key.k.freq_millihz() / 1000;

    switch (current_mode) {
        case OSC1:
        case OSC2:
            // Obtain sound frequency.
            freq *= osc[current_mode].get_freq_shift();

            // Calculate current key phase to select correct oscillator sample value
            key.phase[current_mode] += freq * 0x10000 / Audio::SAMPLING_FREQUENCY;
            sample = osc[current_mode].compute_sample(key.phase[current_mode]);
            break;
        case MASTER:
            return 0;
        default:
            __unreachable();
    }

    return sample;
}

int Synthesizer::synthesize(int16_t *const block, k_timeout_t timeout) {
    k_timepoint_t deadline = sys_timepoint_calc(timeout);

    // NOTE: We don't care about stereo, so send same data to both channels.
    for (unsigned int i = 0; i < Audio::SAMPLES_PER_BLOCK; i += Audio::CHANNEL_COUNT) {
        int16_t sample = 0;

        if (sys_timepoint_expired(deadline)) {
            return -ETIMEDOUT;
        }

        // Get the synthesized sound for every pressed key
        for (unsigned int j = 0; j < MAX_KEYPRESSES; ++j) {
            if (keypresses[j].state == KeyPress::PRESSED &&
                !sys_timepoint_expired(keypresses[j].hold_time)) {
                sample += compute_sample(keypresses[j]);
            } else if (keypresses[j].state == KeyPress::PRESSED &&
                       sys_timepoint_expired(keypresses[j].hold_time)) {
                keypresses[j].state = KeyPress::IDLE;
            }
        }

        for (unsigned int j = 0; j < Audio::CHANNEL_COUNT; ++j) {
            block[i + j] = sample;
        }
    }

    return 0;
}
