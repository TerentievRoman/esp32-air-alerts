#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "alert_state.h"

/* Sound + LED + buttons in their own task. Non-blocking: DFPlayer start-up
 * wait happens inside the task. */
esp_err_t notify_init(void);

/* Queue a track (1..3) to play once; preempts current playback. */
void notify_play(int track);

/* Apply an alert message in the notify task (the only place alert_state
 * changes); staleness is judged against the clock at that moment. */
void notify_apply(const alert_msg_t *m);

/* Stop current playback. */
void notify_stop(void);

/* Play `track` (1..3) at `vol` (0..30) for 10 s, then stop and restore the
 * scheduled volume. A running test is replaced by the new one. false if a
 * real sound is playing now (nothing queued). */
bool notify_test_volume(int track, int vol);

/* Stop a running volume test early. Real sounds are not affected. */
void notify_stop_test(void);
