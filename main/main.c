/*
 * air_alert — сповіщувач повітряної тривоги на ESP32-C3 SuperMini.
 *
 * Зараз: при старті програє сирену (Air-raid-siren.mp3) з SD-картки
 * через DFPlayer Mini.
 *
 * Підключення DFPlayer Mini HW-247:
 *   ESP32-C3 GPIO1 (TX) --[1 кОм]--> DFPlayer RX  (пін 2)
 *   ESP32-C3 GPIO0 (RX) <----------- DFPlayer TX  (пін 3)
 *   5V -> VCC (пін 1), GND -> GND (пін 7)
 * Звук: джек 3,5 мм на активні колонки
 *   DAC_L (пін 5) -> TIP, DAC_R (пін 4) -> RING, GND (пін 7) -> SLEEVE
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "alert_state.h"
#include "dfplayer.h"

static const char *TAG = "air_alert";

#define DFPLAYER_UART    UART_NUM_1
#define DFPLAYER_TX_GPIO 1
#define DFPLAYER_RX_GPIO 0
#define SIREN_VOLUME     25   /* 0..30 */
/* Сирена — перший (єдиний) файл на картці. */
#define SIREN_TRACK      1

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

    const dfplayer_config_t df = {
        .uart_num = DFPLAYER_UART,
        .tx_gpio = DFPLAYER_TX_GPIO,
        .rx_gpio = DFPLAYER_RX_GPIO,
    };
    ESP_ERROR_CHECK(dfplayer_init(&df));

    if (!dfplayer_wait_ready(5000)) {
        ESP_LOGE(TAG, "DFPlayer not ready (check SD card and wiring)");
    }
    dfplayer_set_volume(SIREN_VOLUME);

    ESP_LOGI(TAG, "playing siren (track %d)", SIREN_TRACK);
    dfplayer_play_track(SIREN_TRACK);
    dfplayer_wait_finished(5 * 60 * 1000);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "alive, state=%s", alert_state_name(alert_state_get()));
    }
}
