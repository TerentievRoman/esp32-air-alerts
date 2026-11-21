#pragma once
#include <stdbool.h>
#include "alert_state.h"
#include "button.h"

/* Which track a state transition plays (0 = silent).
 * - entering CLEAR from UNKNOWN (boot, first retained message) is silent;
 * - stale RED/YELLOW (retained, older than CONFIG_AA_STALE_ALERT_SEC) is silent. */
int notify_track_for(alert_state_t old_state, alert_state_t new_state, bool stale);

typedef enum {
    BTN_ACTION_NONE,
    BTN_ACTION_STOP,   /* stop the sound, keep the state */
    BTN_ACTION_ALERT,  /* local RED */
    BTN_ACTION_CLEAR,  /* local CLEAR */
} btn_action_t;

/* While a sound plays every BOOT gesture only stops it (holding BOOT to
 * silence a real siren must not become a false all-clear); when silent,
 * double = alert, long = clear, short = nothing. */
btn_action_t notify_button_action(button_id_t ev, bool playing);
