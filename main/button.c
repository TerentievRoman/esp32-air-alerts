#include "button.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "button_gesture.h"

static const char *TAG = "button";

#define PIN            CONFIG_AA_BUTTON_GPIO
#define POLL_MS        10
#define DEBOUNCE_POLLS 5   /* 50 ms */

static button_cb_t s_cb;
static esp_timer_handle_t s_timer;
static bool s_stable;          /* debounced: true = pressed */
static uint8_t s_count;        /* consecutive polls differing from s_stable */
static gesture_fsm_t s_fsm;
static bool s_gpio_ready;

static void config_gpio(void)
{
    if (s_gpio_ready) return;
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    s_gpio_ready = true;
}

static bool is_pressed(void) { return gpio_get_level(PIN) == 0; }

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void poll(void *arg)
{
    (void)arg;
    bool raw = is_pressed();
    if (raw == s_stable) {
        s_count = 0;
    } else if (++s_count >= DEBOUNCE_POLLS) {
        s_count = 0;
        s_stable = raw;
    }

    button_id_t ev;
    switch (gesture_feed(&s_fsm, s_stable, now_ms())) {
    case GESTURE_SHORT:  ev = BTN_STOP;  break;
    case GESTURE_DOUBLE: ev = BTN_ALERT; break;
    case GESTURE_LONG:   ev = BTN_CLEAR; break;
    default: return;
    }
    ESP_LOGI(TAG, "BOOT %s", ev == BTN_STOP ? "short" : ev == BTN_ALERT ? "double" : "long");
    if (s_cb) s_cb(ev);
}

void button_init(button_cb_t on_press)
{
    config_gpio();
    s_cb = on_press;
    s_stable = is_pressed();
    gesture_init(&s_fsm, s_stable, now_ms());

    const esp_timer_create_args_t args = { .callback = poll, .name = "button" };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_timer, POLL_MS * 1000));
    ESP_LOGI(TAG, "BOOT=GPIO%d: short=stop, double=alert, hold %d ms=clear",
             PIN, GESTURE_LONG_MS);
}

bool button_held_for_ms(button_id_t id, uint32_t ms)
{
    (void)id;
    config_gpio();
    for (uint32_t t = 0; t < ms; t += POLL_MS) {
        if (!is_pressed()) return false;
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    return true;
}
