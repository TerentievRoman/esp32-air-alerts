#pragma once
#include "esp_err.h"

/* Start the LCD task (redraw once a second). Normal mode: sta_ssid is the
 * configured network, ap_ssid NULL. Setup mode: ap_ssid is the setup AP name.
 * Returns an error (and the firmware carries on) if the LCD is disabled in
 * menuconfig or fails to initialise. */
esp_err_t screen_start(const char *sta_ssid, const char *ap_ssid);
