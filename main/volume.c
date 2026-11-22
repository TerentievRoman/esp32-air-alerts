#include "volume.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

#include "clock.h"

static const char *TAG = "volume";
static const char *NS = "aa";
static const char *KEY = "vsched";

#define BLOB_VER 1

typedef struct {
    uint8_t ver;
    volume_sched_t s;
} blob_t;

static SemaphoreHandle_t s_mtx;
static volume_sched_t s_sched;

static void set_default(volume_sched_t *s)
{
    memset(s, 0, sizeof(*s));
    s->def = CONFIG_AA_DF_VOLUME;
    s->n = 0;
}

esp_err_t volume_init(void)
{
    if (!s_mtx) {
        s_mtx = xSemaphoreCreateMutex();
        if (!s_mtx) return ESP_ERR_NO_MEM;
    }

    volume_sched_t loaded;
    set_default(&loaded);

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        blob_t b;
        size_t len = sizeof(b);
        err = nvs_get_blob(h, KEY, &b, &len);
        nvs_close(h);
        if (err == ESP_OK) {
            if (len == sizeof(b) && b.ver == BLOB_VER && volume_sched_valid(&b.s)) {
                loaded = b.s;
            } else {
                ESP_LOGW(TAG, "stored schedule unusable, using default");
            }
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "nvs_get_blob failed: %s, using default", esp_err_to_name(err));
        }
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs_open failed: %s, using default", esp_err_to_name(err));
    }

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_sched = loaded;
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "schedule: %d rules, default %d", loaded.n, loaded.def);
    return ESP_OK;
}

int volume_now(void)
{
    int64_t now = clock_now();
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    int v = volume_at(&s_sched, (time_t)now);
    xSemaphoreGive(s_mtx);
    return v;
}

void volume_get(volume_sched_t *out)
{
    xSemaphoreTake(s_mtx, portMAX_DELAY);
    *out = s_sched;
    xSemaphoreGive(s_mtx);
}

esp_err_t volume_set(const volume_sched_t *s)
{
    if (!s || !volume_sched_valid(s)) return ESP_ERR_INVALID_ARG;

    blob_t b = { .ver = BLOB_VER };
    b.s = *s;

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, KEY, &b, sizeof(b));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return err;

    xSemaphoreTake(s_mtx, portMAX_DELAY);
    s_sched = *s;
    xSemaphoreGive(s_mtx);
    ESP_LOGI(TAG, "schedule saved: %d rules, default %d", s->n, s->def);
    return ESP_OK;
}
