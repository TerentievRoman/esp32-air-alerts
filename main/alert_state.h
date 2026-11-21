#pragma once
#include <stdbool.h>

typedef enum {
    ALERT_STATE_UNKNOWN = 0,   // no data yet (boot / no network)
    ALERT_STATE_CLEAR,         // відбій
    ALERT_STATE_ACTIVE,        // повітряна тривога
} alert_state_t;

void alert_state_init(void);
alert_state_t alert_state_get(void);
/* Returns true if the state actually changed. */
bool alert_state_set(alert_state_t new_state);
const char *alert_state_name(alert_state_t s);
