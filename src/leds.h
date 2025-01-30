#pragma once

enum led {
    LED_DEBUG_0,  // Green
    LED_DEBUG_1,  // Orange
    LED_DEBUG_2,  // Red
    LED_DEBUG_3,  // Blue

    LED_STATUS_0,
    LED_STATUS_1,
    LED_STATUS_2,
    LED_STATUS_3,
    LED_STATUS_4,

    LED_COUNT,
};

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/// @brief LEDs initialization function
/// Call it before using the LEDs
/// @return 0 on success, -ERRNO otherwise
int leds_init(void);

/// @brief Turn on a specific LED
/// @param led to turn on
/// @return 0 on success, -ERRNO otherwise
int led_set(enum led led);

/// @brief Turn off a specific LED
/// @param led to turn on
/// @return 0 on success, -ERRNO otherwise
int led_reset(enum led led);

#ifdef __cplusplus
}
#endif  // __cplusplus
