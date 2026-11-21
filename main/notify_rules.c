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
