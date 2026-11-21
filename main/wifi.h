#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef void (*wifi_event_cb_t)(bool connected);

/* netif, default event loop, STA and AP netifs (created once). */
esp_err_t wifi_init(wifi_event_cb_t cb);

/* Station mode; reconnects with 1, 2, 4 ... 30 s pauses. */
esp_err_t wifi_start_sta(const char *ssid, const char *pass);

/* Setup access point at 192.168.4.1, channel 1, max 2 clients. */
esp_err_t wifi_start_ap(const char *ssid, const char *pass);

bool wifi_is_connected(void);

/* Last 3 bytes of the STA MAC, lowercase hex. */
void wifi_get_device_id(char out[7]);
