#include "alert_state.h"
#include "esp_log.h"

static const char *TAG = "alert_state";
static alert_state_t s_state = ALERT_STATE_UNKNOWN;

void alert_state_init(void) { s_state = ALERT_STATE_UNKNOWN; }

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
    case ALERT_STATE_ACTIVE: return "ACTIVE";
    default:                 return "UNKNOWN";
    }
}
