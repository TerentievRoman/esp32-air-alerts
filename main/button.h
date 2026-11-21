#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Events of the on-board BOOT button (CONFIG_AA_BUTTON_GPIO). */
typedef enum {
    BTN_STOP,   /* short press: stop sound */
    BTN_ALERT,  /* double press: local alert (RED) */
    BTN_CLEAR,  /* hold 2 s: local clear */
    BTN_COUNT,
} button_id_t;

/* Called from the esp_timer task: keep it short (set a flag / post to a queue). */
typedef void (*button_cb_t)(button_id_t id);

/* Active-low BOOT with pull-up, polled every 10 ms, 50 ms debounce,
 * gestures from button_gesture.c. */
void button_init(button_cb_t on_press);

/* Blocking: true if BOOT is held for the whole ms interval (any id maps to BOOT). */
bool button_held_for_ms(button_id_t id, uint32_t ms);
