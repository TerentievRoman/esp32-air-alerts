#pragma once
#include <stdbool.h>
#include "alert_state.h"

/* Which track a state transition plays (0 = silent).
 * - entering CLEAR from UNKNOWN (boot, first retained message) is silent;
 * - stale RED/YELLOW (retained, older than CONFIG_AA_STALE_ALERT_SEC) is silent. */
int notify_track_for(alert_state_t old_state, alert_state_t new_state, bool stale);
