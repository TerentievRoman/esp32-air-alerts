#include "led.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

static const char *TAG = "led";

#define TICK_MS 250

static esp_timer_handle_t s_timer;
static volatile led_pattern_t s_pattern = LED_OFF;
static uint32_t s_tick;

static void led_write(bool on)
{
    gpio_set_level(CONFIG_AA_LED_GPIO, on ? 0 : 1); /* active-low */
}

/* Half-period in ticks for each blink pattern. */
static uint32_t half_period_ticks(led_pattern_t p)
{
    switch (p) {
    case LED_BLINK_2HZ:   return 1;  /* 250 ms on / 250 ms off */
    case LED_BLINK_1HZ:   return 2;  /* 500 / 500 */
    case LED_BLINK_0_5HZ: return 4;  /* 1000 / 1000 */
    default:              return 0;
    }
}

static void led_tick(void *arg)
{
    (void)arg;
    led_pattern_t p = s_pattern;
    uint32_t half = half_period_ticks(p);
    if (half == 0) {
        led_write(p == LED_ON);
        return;
    }
    led_write((s_tick / half) % 2 == 0);
    s_tick++;
}

void led_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_AA_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    led_write(false);

    const esp_timer_create_args_t args = { .callback = led_tick, .name = "led" };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_timer, TICK_MS * 1000));
    ESP_LOGI(TAG, "GPIO%d", CONFIG_AA_LED_GPIO);
}

void led_set(led_pattern_t p)
{
    if (p == s_pattern) return;
    s_tick = 0;
    s_pattern = p;
    ESP_LOGD(TAG, "pattern %d", p);
}
