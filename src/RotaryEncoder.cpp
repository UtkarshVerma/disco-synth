#include "RotaryEncoder.hpp"

#include <errno.h>
#include <sys/cdefs.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

#include <cstdint>

LOG_MODULE_REGISTER(rotary_encoder, LOG_LEVEL_INF);

RotaryEncoder::RotaryEncoder(const Pins pins, const Callback callback)
    : callback(callback), prev_pin_state(0), pins(pins) {}

uint8_t RotaryEncoder::read_pins(void) {
    const bool a = gpio_pin_get_dt(&this->pins.a);
    const bool b = gpio_pin_get_dt(&this->pins.b);

    return (b << 1) | a;
}

int RotaryEncoder::init(void) {
    int ret;

    ret = gpio_pin_configure_dt(&this->pins.a, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to initialize pin A: %d", -ret);
        return ret;
    }

    ret = gpio_pin_configure_dt(&this->pins.b, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Failed to initialize pin B: %d", -ret);
        return ret;
    }

    k_work_init_delayable(&this->dwork, [](struct k_work* const item) {
        struct k_work_delayable* dwork = k_work_delayable_from_work(item);
        RotaryEncoder* sw              = CONTAINER_OF(dwork, RotaryEncoder, dwork);
        sw->update();
    });

    gpio_init_callback(
        &this->a_cb,
        [](const struct device* dev, struct gpio_callback* cb, gpio_port_pins_t pins) {
            RotaryEncoder* enc = CONTAINER_OF(cb, RotaryEncoder, a_cb);
            k_work_schedule(&enc->dwork, K_MSEC(5));
        },
        BIT(pins.a.pin));
    ret = gpio_add_callback(this->pins.a.port, &this->a_cb);
    if (ret < 0) {
        LOG_ERR("Could not add callback to pin A: %d", -ret);
        return ret;
    }

    gpio_init_callback(
        &this->b_cb,
        [](const struct device* dev, struct gpio_callback* cb, gpio_port_pins_t pins) {
            RotaryEncoder* enc = CONTAINER_OF(cb, RotaryEncoder, b_cb);
            k_work_schedule(&enc->dwork, K_MSEC(5));
        },
        BIT(pins.b.pin));
    ret = gpio_add_callback(this->pins.b.port, &this->b_cb);
    if (ret < 0) {
        LOG_ERR("Could not add callbacks to pin B: %d", -ret);
        return ret;
    }

    this->prev_pin_state = this->read_pins();

    ret = gpio_pin_interrupt_configure_dt(&pins.a, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Could not enabled interrupts for pin A: %d", -ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&pins.b, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Could not enabled interrupts for pin B: %d", -ret);
        return ret;
    }

    return 0;
}

int RotaryEncoder::update(void) {
    const uint8_t pin_state = this->read_pins();

    // No change observed.
    if (pin_state == this->prev_pin_state) {
        return 0;
    }

    // The mask follows the following gray code paths:
    // 00 -> 01 -> 11 -> 10 -> 00: Clockwise
    // 00 -> 10 -> 11 -> 01 -> 00: Anticlockwise

    // Input leads to a non-gray code transition, which is ambiguous.
    if ((pin_state & this->prev_pin_state) == 0) {
        LOG_DBG("Reached an ambiguous transition");
        this->prev_pin_state = pin_state;
        return -EINVAL;
    }

    // At this point, the state transition could either cause an increment or
    // decrement.
    bool is_clockwise;
    switch (this->prev_pin_state) {
        case 0b00:
            is_clockwise = pin_state == 0b01;
            break;
        case 0b01:
            is_clockwise = pin_state == 0b11;
            break;
        case 0b10:
            is_clockwise = pin_state == 0b00;
            break;
        case 0b11:
            is_clockwise = pin_state == 0b10;
            break;
        default:
            __unreachable();
    }

    this->prev_pin_state = pin_state;

    if (this->callback != nullptr) {
        this->callback(is_clockwise);
    }

    return 0;
}
