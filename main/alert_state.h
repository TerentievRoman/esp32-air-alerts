#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ALERT_STATE_UNKNOWN = 0,   // no data yet (boot / no network)
    ALERT_STATE_CLEAR,         // відбій
    ALERT_STATE_YELLOW,        // загроза лише від дронів
    ALERT_STATE_RED,           // повітряна тривога
} alert_state_t;

typedef struct {
    alert_state_t state;
    int64_t ts;                // unix time of the event at the source (0 = unknown)
    uint8_t src;               // alert_src_t: 0 MQTT, 1 local button
} alert_msg_t;

typedef enum { ALERT_SRC_MQTT = 0, ALERT_SRC_LOCAL = 1 } alert_src_t;

typedef void (*alert_state_cb_t)(alert_state_t old_state, alert_state_t new_state, bool stale);

void alert_state_init(void);
alert_state_t alert_state_get(void);
/* Returns true if the state actually changed. Does not call the callback. */
bool alert_state_set(alert_state_t new_state);
const char *alert_state_name(alert_state_t s);

/* Parse {"state":"CLEAR|YELLOW|RED","ts":<unix>,...}. len == 0: NUL-terminated.
 * False on invalid JSON, unknown state, missing or non-numeric ts. */
bool alert_state_parse(const char *json, size_t len, alert_msg_t *out);

/* True only if the clock is synced (now > 0) and the message is older than max_age_sec. */
bool alert_state_is_stale(const alert_msg_t *m, int64_t now, int64_t max_age_sec);

void alert_state_set_callback(alert_state_cb_t cb);

/* True for a retained (re-)delivery whose ts is not newer than the last MQTT
 * message already applied (e.g. resubscribe after a reconnect). */
bool alert_msg_is_replay(bool retained, int64_t ts, int64_t last_mqtt_ts);

/* Last message that changed the state (valid inside the callback). */
alert_msg_t alert_state_last_msg(void);

/* Set state from a message; on change call the callback with the stale flag.
 * Returns true if the state changed. */
bool alert_state_apply(const alert_msg_t *m, int64_t now);
