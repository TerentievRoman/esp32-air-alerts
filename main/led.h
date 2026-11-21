#pragma once

typedef enum {
    LED_OFF,
    LED_ON,
    LED_BLINK_1HZ,
    LED_BLINK_2HZ,
    LED_BLINK_0_5HZ,
} led_pattern_t;

/* Status LED on CONFIG_AA_LED_GPIO (active-low), driven by a periodic esp_timer. */
void led_init(void);
void led_set(led_pattern_t p);
