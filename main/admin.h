#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "settings.h"

/* Start the admin web server.
 * captive=true (setup AP): no auth, DNS answers 192.168.4.1 to everything,
 * OS connectivity probes are redirected to "/".
 * captive=false (STA): HTTP Basic Auth, user "admin", password s->admin_pass. */
esp_err_t admin_start(settings_t *s, bool captive);
