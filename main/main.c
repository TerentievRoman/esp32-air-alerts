/*
 * air_alert — сповіщувач повітряної тривоги на ESP32-C3 SuperMini.
 *
 * Каркас проєкту. Модулі, які ще треба додати:
 *   - wifi:   підключення до Wi-Fi (NVS-збережені креденшали або provisioning)
 *   - source: отримання стану тривоги для обраного регіону (HTTPS API)
 *   - notify: індикація (LED / зумер / дисплей)
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "alert_state.h"

static const char *TAG = "air_alert";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    alert_state_init();
    ESP_LOGI(TAG, "air_alert firmware started, state=%s", alert_state_name(alert_state_get()));

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "alive, state=%s", alert_state_name(alert_state_get()));
    }
}
