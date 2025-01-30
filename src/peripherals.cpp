#include "peripherals.hpp"

#include <sys/cdefs.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>

#include "RotaryEncoder.hpp"
#include "Switch.hpp"
#include "Synthesizer.hpp"
#include "USB.hpp"

LOG_MODULE_REGISTER(peripherals, LOG_LEVEL_INF);

enum peripheral_encoder {
    ENCODER_OSC_WAVE,
    ENCODER_OSC_PITCH,
    ENCODER_OSC_VOLUME,

    ENCODER_COUNT,
};

enum peripheral_switch {
    /// @brief Low Pass Filter/Oscillator selection switch
    SWITCH_OSC_SEL,

    /// @brief Switch that selects between LFOs, amplitude modulation and effects
    SWITCH_EFFECTS_SEL,

    /// @brief Switch that selects where the effect is applied
    SWITCH_EFFECTS_TARGET,

    /// @brief Switch that selects internal effects parameters. For instance, for
    /// the LFO, the type of oscillator that you want
    SWITCH_EFFECTS_CONF,

    SWITCH_COUNT,
};

static Switch switches[SWITCH_COUNT] = {
    [SWITCH_OSC_SEL]     = Switch(SWITCH_GPIO_PINS(sw_osc_sel),
                                  [](const Switch::State state) {
                                  Synthesizer::Mode oscillator;
                                  switch (state) {
                                      case Switch::DOWN:
                                          oscillator = Synthesizer::Mode::OSC2;
                                          break;
                                      case Switch::NEUTRAL:
                                          oscillator = Synthesizer::Mode::MASTER;
                                          break;
                                      case Switch::UP:
                                          oscillator = Synthesizer::Mode::OSC1;
                                          break;
                                      default:
                                          __unreachable();
                                  }

                                  Synthesizer::set_mode(oscillator);
                              }),
    [SWITCH_EFFECTS_SEL] = Switch(SWITCH_GPIO_PINS(sw_effects_sel),
                                  [](const Switch::State state) {
                                      Synthesizer::Effect effect;
                                      switch (state) {
                                          case Switch::DOWN:
                                              effect = Synthesizer::Effect::SPECIAL;
                                              break;
                                          case Switch::NEUTRAL:
                                              effect = Synthesizer::Effect::AMP_MOD;
                                              break;
                                          case Switch::UP:
                                              effect = Synthesizer::Effect::LFO_MOD;
                                              break;
                                          default:
                                              __unreachable();
                                      }

                                      Synthesizer::set_effect(effect);
                                  }),
    [SWITCH_EFFECTS_TARGET] =
        Switch(SWITCH_GPIO_PINS(sw_effects_target),
               [](const Switch::State state) {
                   USB::println("Effects target switch not implemented yet.");
               }),
    [SWITCH_EFFECTS_CONF] =
        Switch(SWITCH_GPIO_PINS(sw_effects_conf),
               [](Switch::State state) {
                   USB::println("Effects configuration switch not implemented yet.");
               }),
};

static RotaryEncoder encoders[ENCODER_COUNT] = {
    [ENCODER_OSC_WAVE] =
        RotaryEncoder(ROTARY_ENCODER_PINS(enc_osc_wave), Synthesizer::change_waveform),
    [ENCODER_OSC_PITCH] =
        RotaryEncoder(ROTARY_ENCODER_PINS(enc_osc_pitch), Synthesizer::change_pitch),
    [ENCODER_OSC_VOLUME] =
        RotaryEncoder(ROTARY_ENCODER_PINS(enc_osc_volume), Synthesizer::change_volume),
};

int peripherals_init(void) {
    int ret;

    for (unsigned int i = 0; i < SWITCH_COUNT; ++i) {
        ret = switches[i].init();
        if (ret < 0) {
            LOG_ERR("Failed to initialize switch %d: %d", i, -ret);
            return ret;
        }
    }

    for (unsigned int i = 0; i < ENCODER_COUNT; ++i) {
        ret = encoders[i].init();
        if (ret < 0) {
            LOG_ERR("Failed to initialize encoder %d: %d", i, -ret);
            return ret;
        }
    }

    return 0;
}
