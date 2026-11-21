/*
 * air_alert — сповіщувач повітряної тривоги на ESP32-C3 SuperMini.
 *
 * Тут лише NVS і запуск модулів; уся робота в задачах і таймерах модулів.
 * Звук грає тільки за подіями (тривога, відбій, 09:00, кнопка, тест з адмінки).
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include "admin.h"
#include "alert_state.h"
#include "button.h"
#include "clock.h"
#include "led.h"
#include "mqtt.h"
#include "notify.h"
#include "settings.h"
#include "silence.h"
#include "wifi.h"

static const char *TAG = "air_alert";

/* GPIO9 (BOOT) low at reset selects the ROM download mode, so the reset
 * gesture is: power up / RESET first, then hold BOOT within this window. */
#define RESET_WINDOW_MS 2000

static settings_t s_settings;

static void on_wifi(bool connected)
{
    static bool started;
    if (!connected || started) return;
    started = true;
    clock_init(silence_reschedule);
    silence_init();
    mqtt_start(&s_settings);
}

static bool reset_requested(void)
{
    for (int t = 0; t < RESET_WINDOW_MS; t += 10) {
        if (button_held_for_ms(BTN_STOP, 10)) {
            ESP_LOGW(TAG, "BOOT held: keep holding %d ms to erase settings", CONFIG_AA_RESET_HOLD_MS);
            led_set(LED_BLINK_2HZ);
            bool held = button_held_for_ms(BTN_STOP, CONFIG_AA_RESET_HOLD_MS);
            led_set(LED_OFF);
            return held;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    settings_load(&s_settings);
    led_init();

    if (reset_requested()) {
        ESP_LOGW(TAG, "settings erased by BOOT hold, restarting");
        settings_erase();
        esp_restart();
    }

    alert_state_init();
    ESP_ERROR_CHECK(notify_init());   /* DFPlayer, LED patterns, BOOT gestures, alert log */
    ESP_ERROR_CHECK(wifi_init(on_wifi));

    if (!settings_has_wifi(&s_settings)) {
        char id[7], ssid[32];
        wifi_get_device_id(id);
        snprintf(ssid, sizeof(ssid), "air-alert-%s", id);
        ESP_LOGW(TAG, "no Wi-Fi configured: setup AP '%s'", ssid);
        wifi_start_ap(ssid, CONFIG_AA_AP_PASS);
        admin_start(&s_settings, true);
        led_set(LED_BLINK_2HZ);
        return;
    }

    wifi_start_sta(s_settings.wifi_ssid, s_settings.wifi_pass);
    admin_start(&s_settings, false);
    ESP_LOGI(TAG, "started, state=%s", alert_state_name(alert_state_get()));
}
