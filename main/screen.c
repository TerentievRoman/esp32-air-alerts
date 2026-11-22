#include "screen.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "alert_log.h"
#include "alert_state.h"
#include "clock.h"
#include "lcd.h"
#include "mqtt.h"
#include "screen_fmt.h"
#include "wifi.h"

static const char *TAG = "screen";

#define REFRESH_MS 1000

static screen_info_t s_info;   /* constant parts (mode, SSIDs) filled at start */

static void screen_task(void *arg)
{
    (void)arg;
    screen_lines_t lines;
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        screen_info_t in = s_info;
        if (!in.ap_mode) {
            in.now = clock_now();
            in.state = alert_state_get();
            in.wifi = wifi_is_connected();
            in.mqtt = mqtt_is_connected();
            in.has_log = alert_log_last(&in.last);
        }
        screen_format(&in, &lines);
        lcd_clear();
        for (int r = 0; r < SCREEN_ROWS; r++) lcd_text(0, r, lines.text[r], lines.invert[r]);
        lcd_flush();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(REFRESH_MS));
    }
}

esp_err_t screen_start(const char *sta_ssid, const char *ap_ssid)
{
#if !CONFIG_AA_LCD_ENABLE
    (void)sta_ssid;
    (void)ap_ssid;
    ESP_LOGI(TAG, "LCD disabled in menuconfig");
    return ESP_ERR_NOT_SUPPORTED;
#else
    esp_err_t err = lcd_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed: %s (continuing without screen)", esp_err_to_name(err));
        return err;
    }
    memset(&s_info, 0, sizeof(s_info));
    s_info.ap_mode = ap_ssid != NULL;
    if (ap_ssid) strlcpy(s_info.ap_ssid, ap_ssid, sizeof(s_info.ap_ssid));
    if (sta_ssid) strlcpy(s_info.ssid, sta_ssid, sizeof(s_info.ssid));
    if (xTaskCreate(screen_task, "screen", 3072, NULL, 3, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
#endif
}
