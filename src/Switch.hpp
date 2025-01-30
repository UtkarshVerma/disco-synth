#pragma once

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#define SWITCH_GPIO_PINS(node_label)                              \
    Switch::Pins {                                                \
        .up   = GPIO_DT_SPEC_GET(DT_CHOSEN(node_label), up_pin),  \
        .down = GPIO_DT_SPEC_GET(DT_CHOSEN(node_label), down_pin) \
    }

class Switch {
   public:
    struct k_work_delayable dwork;
    enum State { DOWN, NEUTRAL, UP };
    struct Pins {
        const struct gpio_dt_spec up;
        const struct gpio_dt_spec down;
    };
    // Switch's callback type definition, called when the switch changes its state
    typedef void (*Callback)(State state);

   private:
    struct gpio_callback up_cb;
    struct gpio_callback down_cb;
    Pins pins;
    State last_state;
    Callback callback;

    Switch::State read_state(void);

   public:
    /// @brief Constructor
    /// Call this function during initialization before calling the rest of the fuctions
    /// @param pins Up and down pins
    /// @param callback the callback that is called when the switch changes its state.
    /// no callback
    Switch(Pins pins, Callback callback = nullptr);

    /// @brief Switch's initialization function
    /// @return 0 on success, -ERRNO otherwise
    int init(void);

    /// @brief Switch update function.
    void update(void);
};
