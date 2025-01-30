#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/kernel/thread_stack.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/sys_clock.h>

#include <cstddef>

#include "Audio.hpp"
#include "KeyPress.hpp"
#include "Synthesizer.hpp"
#include "Synthesizer/Key.hpp"
#include "USB.hpp"
#include "leds.h"
#include "peripherals.hpp"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

constexpr size_t STACK_SIZE = 1024;

struct k_thread synth_thread;
struct k_thread keyboard_thread;

K_THREAD_STACK_DEFINE(synth_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(keyboard_stack, STACK_SIZE);

/// Function that checks key presses
static void check_keyboard(void) {
    char character;
    while (USB::read(&character, 1) != 0) {
        auto key         = Key(character);
        bool key_pressed = false;
        for (unsigned int i = 0; i < MAX_KEYPRESSES; ++i) {
            if (key == keypresses[i].k && keypresses[i].state != KeyPress::IDLE) {
                keypresses[i].state        = KeyPress::PRESSED;
                keypresses[i].hold_time    = sys_timepoint_calc(K_MSEC(500));
                keypresses[i].release_time = sys_timepoint_calc(K_MSEC(500));
                key_pressed                = true;
            }
        }
        // The second loop is necessary to avoid selecting an IDLE key when a
        // PRESSED or RELEASED key is located further away on the array
        if (!key_pressed) {
            for (unsigned int i = 0; i < MAX_KEYPRESSES; ++i) {
                if (keypresses[i].state == KeyPress::IDLE) {
                    keypresses[i].k            = key;
                    keypresses[i].state        = KeyPress::PRESSED;
                    keypresses[i].hold_time    = sys_timepoint_calc(K_MSEC(500));
                    keypresses[i].release_time = sys_timepoint_calc(K_MSEC(500));
                    keypresses[i].phase[0]     = 0;
                    keypresses[i].phase[1]     = 0;
                    break;
                }
            }
        }
    }
}

static inline int prepare_buffer(const k_timeout_t alloc_timeout,
                                 const k_timeout_t synth_timeout) {
    const auto block = Audio::get_block(alloc_timeout);
    if (block == nullptr) {
        return -ENOMEM;
    }

    return Synthesizer::synthesize(block, synth_timeout);
}

static void synth_thread_func(void *arg1, void *arg2, void *arg3) {
    // Fill the audio TX queue to ensure further allocs block until DMA free.
    for (unsigned int i = 0; i < Audio::BLOCK_COUNT; ++i) {
        prepare_buffer(K_NO_WAIT, K_FOREVER);
        Audio::write_block();
    }

    int ret = Audio::start_writes();
    if (ret < 0) {
        LOG_ERR("Failed to start audio writes: %d", -ret);
        return;
    }

    bool overload_led_set = false;
    while (true) {
        // NOTE:
        // Since the TX queue is filled at this point, this call will block
        // until the DMA is done and frees one the pending block. This will
        // never take longer than Audio::BLOCK_DURATION_MS. After getting this
        // block, to keep the I2S queue hydrated, another transfer MUST be
        // queued within Audio::BLOCK_DURATION_MS.
        //
        // While blocking, CPU is yielded to lower priority tasks.
        // Ensure that synthizer leaves a 20 ms slack.
        ret = prepare_buffer(K_FOREVER, K_MSEC(Audio::BLOCK_DURATION_MS - 20));
        if (ret == -ETIMEDOUT) {
            Audio::clear_block();
            (void)led_set(LED_STATUS_4);
            overload_led_set = true;
        } else if (overload_led_set) {
            led_reset(LED_STATUS_4);
            overload_led_set = false;
        }

        led_set(LED_STATUS_1);
        Audio::write_block();
        led_reset(LED_STATUS_1);
    }
}

int main(void) {
    int ret;

    constexpr auto usb_dev = DEVICE_DT_GET_ONE(zephyr_cdc_acm_uart);
    ret                    = USB::init(usb_dev);
    if (ret < 0) {
        LOG_ERR("USB initialization failed: %d", -ret);
        return ret;
    }

    ret = leds_init();
    if (ret < 0) {
        USB::println("LEDs initialization failed: %d", -ret);
        return ret;
    }

    constexpr auto audio_i2s_dev   = DEVICE_DT_GET(DT_CHOSEN(audio_i2s));
    constexpr auto audio_codec_dev = DEVICE_DT_GET(DT_CHOSEN(audio_codec));
    ret                            = Audio::init(audio_codec_dev, audio_i2s_dev);
    if (ret < 0) {
        USB::println("Audio initialization failed: %d", -ret);
        return ret;
    }

    ret = peripherals_init();
    if (ret < 0) {
        USB::println("Peripheral initialization failed: %d", -ret);
        return ret;
    }

    Synthesizer::init();

    (void)USB::println("== Synthesizer up and running ==");

    (void)k_thread_create(&synth_thread, synth_stack, STACK_SIZE, synth_thread_func, nullptr,
                          NULL, NULL, -1, 0, K_NO_WAIT);

    (void)k_thread_create(
        &keyboard_thread, keyboard_stack, STACK_SIZE,
        [](void *_a, void *_b, void *_c) {
            while (true) {
                led_set(LED_DEBUG_1);
                check_keyboard();
                led_reset(LED_DEBUG_1);

                k_msleep(20);
            }
        },
        nullptr, nullptr, nullptr, 2, 0, K_NO_WAIT);

    return 0;
}
