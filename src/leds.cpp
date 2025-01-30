#include "leds.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_core.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(leds, LOG_LEVEL_INF);
#define LED(node_label) GPIO_DT_SPEC_GET(DT_CHOSEN(node_label), gpios)

static const struct gpio_dt_spec leds[LED_COUNT] = {
    [LED_DEBUG_0] = LED(led_debug_0),   [LED_DEBUG_1] = LED(led_debug_1),
    [LED_DEBUG_2] = LED(led_debug_2),   [LED_DEBUG_3] = LED(led_debug_3),

    [LED_STATUS_0] = LED(led_status_0), [LED_STATUS_1] = LED(led_status_1),
    [LED_STATUS_2] = LED(led_status_2), [LED_STATUS_3] = LED(led_status_3),
    [LED_STATUS_4] = LED(led_status_4),
};

int leds_init(void) {
    int ret;

    for (unsigned int i = 0; i < LED_COUNT; ++i) {
        const struct gpio_dt_spec* const led = &leds[i];
        if (!gpio_is_ready_dt(led)) {
            LOG_ERR("GPIO led was not ready: %d", i);
            return -EBUSY;
        }

        ret = gpio_pin_configure_dt(led, GPIO_OUTPUT_ACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure LED %d: %d", i, -ret);
            return ret;
        }

        ret = gpio_pin_set_dt(led, false);
        if (ret < 0) {
            LOG_ERR("Failed to set LED %d: %d", i, -ret);
            return ret;
        }
    }

    return 0;
}

int led_set(enum led led) {
    return gpio_pin_set_dt(&leds[led], true);
}

int led_reset(enum led led) {
    return gpio_pin_set_dt(&leds[led], false);
}
