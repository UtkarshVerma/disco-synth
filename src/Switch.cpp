#include "Switch.hpp"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/dt-bindings/gpio/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/util_macro.h>

LOG_MODULE_REGISTER(switch, LOG_LEVEL_INF);

Switch::State Switch::read_state(void) {
    const bool up   = gpio_pin_get_dt(&this->pins.up);
    const bool down = gpio_pin_get_dt(&this->pins.down);

    State state;
    if (up == down) {
        state = State::NEUTRAL;
    } else if (up) {
        state = State::UP;
    } else {
        state = State::DOWN;
    }

    return state;
}

int Switch::init(void) {
    int ret;

    if (!gpio_is_ready_dt(&this->pins.up)) {
        LOG_ERR("GPIO up was not ready");
        return -EBUSY;
    }
    ret = gpio_pin_configure_dt(&this->pins.up, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        LOG_ERR("Could not configure up GPIO: %d", -ret);
        return ret;
    }

    if (!gpio_is_ready_dt(&this->pins.down)) {
        LOG_ERR("GPIO down was not ready");
        return -EBUSY;
    }
    ret = gpio_pin_configure_dt(&this->pins.down, GPIO_INPUT | GPIO_PULL_UP);
    if (ret < 0) {
        LOG_ERR("Could not configure up GPIO: %d", -ret);
        return ret;
    }

    k_work_init_delayable(&this->dwork, [](struct k_work* const item) {
        struct k_work_delayable* dwork = k_work_delayable_from_work(item);
        Switch* sw                     = CONTAINER_OF(dwork, Switch, dwork);
        sw->update();
    });

    gpio_init_callback(
        &this->up_cb,
        [](const struct device* dev, struct gpio_callback* cb, gpio_port_pins_t pins) {
            Switch* sw = CONTAINER_OF(cb, Switch, up_cb);
            k_work_schedule(&sw->dwork, K_MSEC(10));
        },
        BIT(pins.up.pin));
    ret = gpio_add_callback(this->pins.up.port, &this->up_cb);
    if (ret < 0) {
        LOG_ERR("Could not add callback to up pin: %d", -ret);
        return ret;
    }

    gpio_init_callback(
        &this->down_cb,
        [](const struct device* dev, struct gpio_callback* cb, gpio_port_pins_t pins) {
            Switch* sw = CONTAINER_OF(cb, Switch, down_cb);
            k_work_schedule(&sw->dwork, K_MSEC(10));
        },
        BIT(pins.down.pin));
    ret = gpio_add_callback(this->pins.down.port, &this->down_cb);
    if (ret < 0) {
        LOG_ERR("Could not add callbacks to down pin: %d", -ret);
        return ret;
    }

    this->last_state = this->read_state();

    ret = gpio_pin_interrupt_configure_dt(&pins.up, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Could not enabled interrupts for up pin: %d", -ret);
        return ret;
    }

    ret = gpio_pin_interrupt_configure_dt(&pins.down, GPIO_INT_EDGE_BOTH);
    if (ret < 0) {
        LOG_ERR("Could not enabled interrupts for down pin: %d", -ret);
        return ret;
    }

    return 0;
}

void Switch::update(void) {
    State state = this->read_state();
    if (state != this->last_state && this->callback != nullptr) {
        this->callback(state);
    }

    this->last_state = state;
}

Switch::Switch(Pins pins, Callback callback)
    : pins(pins), last_state(NEUTRAL), callback(callback) {}
