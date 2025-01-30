#pragma once

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <cstdint>

#define ROTARY_ENCODER_PINS(node_label)                      \
    RotaryEncoder::Pins {                                    \
        .a = GPIO_DT_SPEC_GET(DT_CHOSEN(node_label), a_pin), \
        .b = GPIO_DT_SPEC_GET(DT_CHOSEN(node_label), b_pin)  \
    }

class RotaryEncoder {
   public:
    struct k_work_delayable dwork;
    typedef void (*Callback)(bool is_clockwise);
    typedef struct {
        const struct gpio_dt_spec a;
        const struct gpio_dt_spec b;
    } Pins;

   private:
    struct gpio_callback a_cb;
    struct gpio_callback b_cb;
    Callback callback;
    uint8_t prev_pin_state;
    Pins pins;

    uint8_t read_pins(void);

   public:
    /// @brief Encoder constructor
    /// @param pins Encoder pins
    /// @param min_value The minimum value that the encoder state can reach
    /// @param max_value The maximum value that the encoder state can reach
    /// @param callback Callback to be executed on state change.
    RotaryEncoder(Pins pins, Callback callback = nullptr);

    /// @brief Encoder initialization function
    /// Call this function during initialization
    /// @return Returns 0 on success, -EINVAL if invalid bounds (lower >= upper)
    int init(void);

    /// @brief Update the encoder
    /// @param pin0
    /// @param pin1
    int update(void);
};
