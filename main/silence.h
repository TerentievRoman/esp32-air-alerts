#pragma once

/* Daily minute of silence: track 3 at 09:00:00 Kyiv, skipped during RED.
 * Enabled by CONFIG_AA_SILENCE_ENABLE. */
void silence_init(void);

/* (Re)arm the timer for the next 09:00; pass as clock_init's on_sync.
 * Does nothing while the clock is not synced. */
void silence_reschedule(void);
