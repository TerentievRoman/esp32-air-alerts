#include "alert_state.h"

#include <string.h>
#include "cJSON.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "alert_state";
static alert_state_t s_state = ALERT_STATE_UNKNOWN;
static alert_state_cb_t s_cb;
static alert_msg_t s_last;

void alert_state_init(void)
{
    s_state = ALERT_STATE_UNKNOWN;
    s_last = (alert_msg_t){ 0 };
}

alert_state_t alert_state_get(void) { return s_state; }

bool alert_state_set(alert_state_t new_state)
{
    if (new_state == s_state) return false;
    ESP_LOGI(TAG, "%s -> %s", alert_state_name(s_state), alert_state_name(new_state));
    s_state = new_state;
    return true;
}

const char *alert_state_name(alert_state_t s)
{
    switch (s) {
    case ALERT_STATE_CLEAR:  return "CLEAR";
    case ALERT_STATE_YELLOW: return "YELLOW";
    case ALERT_STATE_RED:    return "RED";
    default:                 return "UNKNOWN";
    }
}

static bool state_from_name(const char *name, alert_state_t *out)
{
    if (strcmp(name, "CLEAR") == 0)  { *out = ALERT_STATE_CLEAR;  return true; }
    if (strcmp(name, "YELLOW") == 0) { *out = ALERT_STATE_YELLOW; return true; }
    if (strcmp(name, "RED") == 0)    { *out = ALERT_STATE_RED;    return true; }
    return false;
}

bool alert_state_parse(const char *json, size_t len, alert_msg_t *out)
{
    if (!json || !out) return false;
    if (len == 0) len = strlen(json);

    cJSON *root = cJSON_ParseWithLength(json, len);
    if (!root) return false;

    bool ok = false;
    const cJSON *st = cJSON_GetObjectItemCaseSensitive(root, "state");
    const cJSON *ts = cJSON_GetObjectItemCaseSensitive(root, "ts");
    alert_msg_t m;
    if (cJSON_IsString(st) && st->valuestring && state_from_name(st->valuestring, &m.state)
        && cJSON_IsNumber(ts)) {
        m.ts = (int64_t)ts->valuedouble;
        m.src = ALERT_SRC_MQTT;
        *out = m;
        ok = true;
    }
    cJSON_Delete(root);
    return ok;
}

bool alert_state_is_stale(const alert_msg_t *m, int64_t now, int64_t max_age_sec)
{
    return now > 0 && now - m->ts > max_age_sec;
}

alert_msg_t alert_state_last_msg(void) { return s_last; }

void alert_state_set_callback(alert_state_cb_t cb) { s_cb = cb; }

bool alert_state_apply(const alert_msg_t *m, int64_t now)
{
    alert_state_t old = s_state;
    if (!alert_state_set(m->state)) return false;
    s_last = *m;
    bool stale = alert_state_is_stale(m, now, CONFIG_AA_STALE_ALERT_SEC);
    if (s_cb) s_cb(old, m->state, stale);
    return true;
}
