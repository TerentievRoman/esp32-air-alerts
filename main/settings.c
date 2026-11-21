#include "settings.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
static const char *NS = "aa";

typedef struct {
    const char *key;
    size_t offset;
    size_t size;
    const char *def;
} field_t;

#define FIELD(k, member, d) { k, offsetof(settings_t, member), sizeof(((settings_t *)0)->member), d }

static const field_t FIELDS[] = {
    FIELD("ssid",   wifi_ssid,  CONFIG_AA_WIFI_SSID),
    FIELD("wpass",  wifi_pass,  CONFIG_AA_WIFI_PASS),
    FIELD("muri",   mqtt_uri,   CONFIG_AA_MQTT_URI),
    FIELD("muser",  mqtt_user,  CONFIG_AA_MQTT_USER),
    FIELD("mpass",  mqtt_pass,  CONFIG_AA_MQTT_PASS),
    FIELD("mtopic", mqtt_topic, CONFIG_AA_MQTT_TOPIC),
    FIELD("apass",  admin_pass, CONFIG_AA_ADMIN_PASS),
};
#define N_FIELDS (sizeof(FIELDS) / sizeof(FIELDS[0]))

esp_err_t settings_load(settings_t *s)
{
    memset(s, 0, sizeof(*s));

    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READONLY, &h);
    bool have_nvs = (err == ESP_OK);
    if (!have_nvs && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs_open: %s, using defaults", esp_err_to_name(err));
    }

    for (size_t i = 0; i < N_FIELDS; i++) {
        char *dst = (char *)s + FIELDS[i].offset;
        size_t len = FIELDS[i].size;
        if (have_nvs && nvs_get_str(h, FIELDS[i].key, dst, &len) == ESP_OK) continue;
        strlcpy(dst, FIELDS[i].def, FIELDS[i].size);
    }

    if (have_nvs) nvs_close(h);
    return ESP_OK;
}

esp_err_t settings_save(const settings_t *s)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    for (size_t i = 0; i < N_FIELDS && err == ESP_OK; i++) {
        err = nvs_set_str(h, FIELDS[i].key, (const char *)s + FIELDS[i].offset);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK) ESP_LOGI(TAG, "saved (ssid='%s')", s->wifi_ssid);
    else ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    return err;
}

esp_err_t settings_erase(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGW(TAG, "settings erased");
    return err;
}

bool settings_has_wifi(const settings_t *s) { return s->wifi_ssid[0] != '\0'; }
