#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char mqtt_uri[128];
    char mqtt_user[64];
    char mqtt_pass[64];
    char mqtt_topic[96];
    char admin_pass[33];
} settings_t;

/* Read namespace "aa"; missing keys get CONFIG_AA_* defaults. */
esp_err_t settings_load(settings_t *s);
esp_err_t settings_save(const settings_t *s);
esp_err_t settings_erase(void);
bool settings_has_wifi(const settings_t *s);
