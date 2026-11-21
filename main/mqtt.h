#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "settings.h"

/* TLS client (certificate bundle), LWT "offline" on
 * air_alert/kyiv/device/<id>/status, subscribes to s->mqtt_topic (QoS 1)
 * and feeds alert_state_parse/apply. */
esp_err_t mqtt_start(const settings_t *s);

bool mqtt_is_connected(void);

/* 0 while connected; otherwise uptime seconds when the connection was lost
 * (or when the client started, if it never connected). */
int64_t mqtt_disconnected_since(void);
