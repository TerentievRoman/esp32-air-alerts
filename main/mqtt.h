#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "settings.h"

/* TLS client (certificate bundle), LWT "offline" on
 * air_alert/kyiv/device/<id>/status, subscribes to s->mqtt_topic (QoS 1)
 * and feeds alert_state_parse/apply. */
esp_err_t mqtt_start(const settings_t *s);

/* Call in STA mode before Wi-Fi starts: from now on the device counts as
 * "broker lost" until the first connection (offline LED even if Wi-Fi never
 * comes up). Not called in setup-AP mode. */
void mqtt_expect(void);

/* Call from the clock's on_sync: applies a retained message that arrived
 * before the time was known, so its staleness can be judged. */
void mqtt_on_time_sync(void);

bool mqtt_is_connected(void);

/* 0 while connected; otherwise uptime seconds when the connection was lost
 * (or when the client started, if it never connected). */
int64_t mqtt_disconnected_since(void);
