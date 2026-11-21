#include "alert_log.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"

#include "alert_log_buf.h"
#include "clock.h"

static const char *TAG = "alert_log";
static const char *NS = "aa_log";
static const char *KEY = "log";

static alert_log_buf_t s_buf;
static SemaphoreHandle_t s_lock;

static esp_err_t persist(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, KEY, &s_buf, sizeof(s_buf));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    return err;
}

esp_err_t alert_log_init(void)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    memset(&s_buf, 0, sizeof(s_buf));

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(s_buf);
        esp_err_t err = nvs_get_blob(h, KEY, &s_buf, &len);
        nvs_close(h);
        if (err != ESP_OK || len != sizeof(s_buf) || s_buf.count > ALERT_LOG_CAP) {
            if (err != ESP_ERR_NVS_NOT_FOUND) ESP_LOGW(TAG, "stored log unusable, starting empty");
            memset(&s_buf, 0, sizeof(s_buf));
        }
    }
    ESP_LOGI(TAG, "%u entries", s_buf.count);
    return ESP_OK;
}

esp_err_t alert_log_record(alert_state_t s, int64_t ts, alert_src_t src)
{
    const alert_log_entry_t x = { .ts = ts, .state = (uint8_t)s, .src = (uint8_t)src };
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool added = alert_log_buf_add(&s_buf, x, clock_now());
    esp_err_t err = added ? persist() : ESP_OK;
    xSemaphoreGive(s_lock);
    if (added) ESP_LOGI(TAG, "logged %s ts %lld (%u entries)", alert_state_name(s), (long long)ts, s_buf.count);
    return err;
}

int alert_log_to_json(char *out, size_t len)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    size_t n = 0;
    int r = snprintf(out, len, "[");
    n += r;
    for (int i = s_buf.count - 1; i >= 0 && n < len; i--) {
        const alert_log_entry_t *x = &s_buf.e[i];
        r = snprintf(out + n, len - n, "%s{\"ts\":%lld,\"state\":\"%s\",\"src\":\"%s\"}",
                     i == s_buf.count - 1 ? "" : ",", (long long)x->ts,
                     alert_state_name((alert_state_t)x->state),
                     x->src == ALERT_SRC_LOCAL ? "local" : "mqtt");
        n += r;
    }
    if (n < len) n += snprintf(out + n, len - n, "]");
    xSemaphoreGive(s_lock);
    return n < len ? (int)n : -1;
}

esp_err_t alert_log_clear(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(&s_buf, 0, sizeof(s_buf));
    esp_err_t err = persist();
    xSemaphoreGive(s_lock);
    ESP_LOGW(TAG, "cleared");
    return err;
}
