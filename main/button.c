#include "button.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "button";

#define POLL_MS       10
#define DEBOUNCE_POLLS 5   /* 50 ms */

static const int PINS[BTN_COUNT] = {
    [BTN_STOP]  = CONFIG_AA_BUTTON_GPIO,
    [BTN_ALERT] = CONFIG_AA_BTN_ALERT_GPIO,
    [BTN_CLEAR] = CONFIG_AA_BTN_CLEAR_GPIO,
};

static button_cb_t s_cb;
static esp_timer_handle_t s_timer;
static bool s_stable[BTN_COUNT];      /* debounced: true = pressed */
static uint8_t s_count[BTN_COUNT];    /* consecutive polls differing from s_stable */
static bool s_gpio_ready;

static void config_gpio(void)
{
    if (s_gpio_ready) return;
    uint64_t mask = 0;
    for (int i = 0; i < BTN_COUNT; i++) mask |= 1ULL << PINS[i];
    const gpio_config_t io = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    s_gpio_ready = true;
}

static bool is_pressed(button_id_t id) { return gpio_get_level(PINS[id]) == 0; }

static void poll(void *arg)
{
    (void)arg;
    for (int i = 0; i < BTN_COUNT; i++) {
        bool raw = is_pressed(i);
        if (raw == s_stable[i]) {
            s_count[i] = 0;
            continue;
        }
        if (++s_count[i] < DEBOUNCE_POLLS) continue;
        s_count[i] = 0;
        s_stable[i] = raw;
        if (raw) {
            ESP_LOGD(TAG, "press %d", i);
            if (s_cb) s_cb(i);
        }
    }
}

void button_init(button_cb_t on_press)
{
    config_gpio();
    s_cb = on_press;
    /* Buttons already held at init must be released first to count as a press. */
    for (int i = 0; i < BTN_COUNT; i++) s_stable[i] = is_pressed(i);

    const esp_timer_create_args_t args = { .callback = poll, .name = "button" };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_timer, POLL_MS * 1000));
    ESP_LOGI(TAG, "STOP=GPIO%d ALERT=GPIO%d CLEAR=GPIO%d",
             PINS[BTN_STOP], PINS[BTN_ALERT], PINS[BTN_CLEAR]);
}

bool button_held_for_ms(button_id_t id, uint32_t ms)
{
    config_gpio();
    for (uint32_t t = 0; t < ms; t += POLL_MS) {
        if (!is_pressed(id)) return false;
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    return true;
}
