#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BTN_STOP,   /* BOOT: stop sound; hold at power-up to erase settings */
    BTN_ALERT,  /* local "alert" */
    BTN_CLEAR,  /* local "clear" */
    BTN_COUNT,
} button_id_t;

/* Called from the esp_timer task: keep it short (set a flag / post to a queue). */
typedef void (*button_cb_t)(button_id_t id);

/* Active-low buttons with pull-ups, polled every 10 ms, 50 ms debounce. */
void button_init(button_cb_t on_press);

/* Blocking: true if the button is held for the whole ms interval. */
bool button_held_for_ms(button_id_t id, uint32_t ms);
