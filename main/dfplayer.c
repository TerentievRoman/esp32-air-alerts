#include "dfplayer.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "dfplayer";

#define DF_START   0x7E
#define DF_VER     0xFF
#define DF_LEN     0x06
#define DF_END     0xEF
#define DF_FRAME   10

#define CMD_PLAY_TRACK   0x03
#define CMD_VOLUME       0x06
#define CMD_SET_SOURCE   0x09
#define CMD_STOP         0x16

#define EVT_TF_FINISHED  0x3D
#define EVT_INIT_PARAMS  0x3F
#define EVT_ERROR        0x40
#define EVT_ACK          0x41
#define EVT_TF_INSERTED  0x3A
#define EVT_TF_REMOVED   0x3B

#define SRC_TF 0x02

static uart_port_t s_uart = UART_NUM_MAX;

static esp_err_t send_cmd(uint8_t cmd, uint16_t param)
{
    if (s_uart == UART_NUM_MAX) return ESP_ERR_INVALID_STATE;

    uint8_t f[DF_FRAME] = {
        DF_START, DF_VER, DF_LEN, cmd, 0x00 /* без ACK */,
        (uint8_t)(param >> 8), (uint8_t)param, 0, 0, DF_END,
    };
    uint16_t sum = 0;
    for (int i = 1; i <= 6; i++) sum += f[i];
    sum = (uint16_t)(0 - sum);
    f[7] = sum >> 8;
    f[8] = sum & 0xFF;

    int n = uart_write_bytes(s_uart, f, sizeof(f));
    ESP_LOGD(TAG, "-> cmd 0x%02X param %u", cmd, param);
    /* DFPlayer пропускає команди, надіслані надто швидко одна за одною. */
    vTaskDelay(pdMS_TO_TICKS(100));
    return n == sizeof(f) ? ESP_OK : ESP_FAIL;
}

/* Читає один валідний кадр. Повертає true і заповнює cmd/param. */
static bool read_frame(uint8_t *cmd, uint16_t *param, uint32_t timeout_ms)
{
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
    uint8_t f[DF_FRAME];
    int pos = 0;

    while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
        uint8_t b;
        if (uart_read_bytes(s_uart, &b, 1, pdMS_TO_TICKS(20)) != 1) continue;

        if (pos == 0 && b != DF_START) continue;
        f[pos++] = b;
        if (pos < DF_FRAME) continue;
        pos = 0;

        if (f[1] != DF_VER || f[2] != DF_LEN || f[9] != DF_END) continue;
        uint16_t sum = 0;
        for (int i = 1; i <= 6; i++) sum += f[i];
        if ((uint16_t)(sum + ((f[7] << 8) | f[8])) != 0) {
            ESP_LOGW(TAG, "bad checksum");
            continue;
        }
        *cmd = f[3];
        *param = (f[5] << 8) | f[6];
        ESP_LOGD(TAG, "<- evt 0x%02X param %u", *cmd, *param);
        return true;
    }
    return false;
}

static const char *error_name(uint16_t code)
{
    switch (code) {
    case 1: return "module busy / card not found";
    case 2: return "sleep mode";
    case 3: return "serial frame error";
    case 4: return "checksum error";
    case 5: return "track out of range";
    case 6: return "track not found";
    case 7: return "insertion error";
    case 8: return "SD card read error";
    case 10: return "entered sleep";
    default: return "unknown";
    }
}

esp_err_t dfplayer_init(const dfplayer_config_t *cfg)
{
    const uart_config_t uc = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(cfg->uart_num, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;
    err = uart_param_config(cfg->uart_num, &uc);
    if (err != ESP_OK) return err;
    err = uart_set_pin(cfg->uart_num, cfg->tx_gpio, cfg->rx_gpio,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    s_uart = cfg->uart_num;
    ESP_LOGI(TAG, "UART%d TX=GPIO%d RX=GPIO%d", cfg->uart_num, cfg->tx_gpio, cfg->rx_gpio);
    return ESP_OK;
}

bool dfplayer_wait_ready(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    uint8_t cmd;
    uint16_t param;

    while (pdTICKS_TO_MS(xTaskGetTickCount() - start) < timeout_ms) {
        uint32_t left = timeout_ms - pdTICKS_TO_MS(xTaskGetTickCount() - start);
        if (!read_frame(&cmd, &param, left)) break;
        if (cmd == EVT_INIT_PARAMS) {
            ESP_LOGI(TAG, "ready, devices mask=0x%02X%s", param,
                     (param & SRC_TF) ? " (SD card present)" : " (NO SD card!)");
            send_cmd(CMD_SET_SOURCE, SRC_TF);
            return (param & SRC_TF) != 0;
        }
        if (cmd == EVT_ERROR) {
            ESP_LOGE(TAG, "error %u: %s", param, error_name(param));
            return false;
        }
    }
    /* Якщо плеєр уже був увімкнений до старту ESP, 0x3F ми пропустили. */
    ESP_LOGW(TAG, "no init message from DFPlayer, assuming it is already up");
    send_cmd(CMD_SET_SOURCE, SRC_TF);
    vTaskDelay(pdMS_TO_TICKS(300));
    return true;
}

esp_err_t dfplayer_set_volume(uint8_t vol)
{
    if (vol > 30) vol = 30;
    return send_cmd(CMD_VOLUME, vol);
}

esp_err_t dfplayer_play_track(uint16_t n)
{
    uart_flush_input(s_uart);
    return send_cmd(CMD_PLAY_TRACK, n);
}

esp_err_t dfplayer_stop(void)
{
    return send_cmd(CMD_STOP, 0);
}

bool dfplayer_wait_finished(uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    uint8_t cmd;
    uint16_t param;

    while (pdTICKS_TO_MS(xTaskGetTickCount() - start) < timeout_ms) {
        uint32_t left = timeout_ms - pdTICKS_TO_MS(xTaskGetTickCount() - start);
        if (!read_frame(&cmd, &param, left)) break;
        switch (cmd) {
        case EVT_TF_FINISHED:
            ESP_LOGI(TAG, "track %u finished", param);
            return true;
        case EVT_ERROR:
            ESP_LOGE(TAG, "error %u: %s", param, error_name(param));
            return false;
        case EVT_TF_REMOVED:
            ESP_LOGE(TAG, "SD card removed");
            return false;
        default:
            break;
        }
    }
    ESP_LOGD(TAG, "timeout waiting for track end");
    return false;
}
