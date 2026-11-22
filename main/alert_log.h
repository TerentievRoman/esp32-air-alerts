#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "alert_state.h"
#include "alert_log_buf.h"

/* Weekly alert log persisted in NVS (namespace "aa_log", blob "log"). */
esp_err_t alert_log_init(void);

/* Append a transition and persist it (no-op for UNKNOWN / duplicates).
 * Call only from the notify task. */
esp_err_t alert_log_record(alert_state_t s, int64_t ts, alert_src_t src);

/* [{"ts":..,"state":"RED","src":"mqtt"|"local"},...], newest first.
 * Returns the length, or -1 if it does not fit. Thread-safe. */
int alert_log_to_json(char *out, size_t len);

esp_err_t alert_log_clear(void);

/* Newest entry; false if the log is empty or not initialised. Thread-safe. */
bool alert_log_last(alert_log_entry_t *out);
