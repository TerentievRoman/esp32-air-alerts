#pragma once
#include "esp_err.h"

/* Sound + LED + buttons in their own task. Non-blocking: DFPlayer start-up
 * wait happens inside the task. */
esp_err_t notify_init(void);

/* Queue a track (1..3) to play once; preempts current playback. */
void notify_play(int track);

/* Stop current playback. */
void notify_stop(void);
