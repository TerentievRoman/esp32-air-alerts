#include "notify_rules.h"

int notify_track_for(alert_state_t old_state, alert_state_t new_state, bool stale)
{
    switch (new_state) {
    case ALERT_STATE_RED:
    case ALERT_STATE_YELLOW:
        return stale ? 0 : 1;
    case ALERT_STATE_CLEAR:
        return old_state == ALERT_STATE_UNKNOWN ? 0 : 2;
    default:
        return 0;
    }
}

btn_action_t notify_button_action(button_id_t ev, bool playing)
{
    if (playing) return BTN_ACTION_STOP;
    switch (ev) {
    case BTN_ALERT: return BTN_ACTION_ALERT;
    case BTN_CLEAR: return BTN_ACTION_CLEAR;
    default:        return BTN_ACTION_NONE;
    }
}
