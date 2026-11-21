#pragma once
/* Weekly alert log ring buffer: pure logic, no ESP headers (host-tested). */
#include <stdbool.h>
#include <stdint.h>
#include "alert_state.h"

#define ALERT_LOG_CAP         128
#define ALERT_LOG_MAX_AGE_SEC (7 * 24 * 3600)

typedef struct {
    int64_t ts;     /* unix time, 0 = unknown */
    uint8_t state;  /* alert_state_t */
    uint8_t src;    /* alert_src_t */
} alert_log_entry_t;

typedef struct {
    uint16_t count;
    alert_log_entry_t e[ALERT_LOG_CAP];  /* e[0] is the oldest */
} alert_log_buf_t;

/* Append (after pruning). False and no change if state is UNKNOWN or the last
 * entry has the same state and a ts not older than x.ts (replay). */
bool alert_log_buf_add(alert_log_buf_t *b, alert_log_entry_t x, int64_t now);

/* Drop entries with ts > 0 older than ALERT_LOG_MAX_AGE_SEC relative to
 * now (or, if now == 0, to the newest ts in the buffer). */
void alert_log_buf_prune(alert_log_buf_t *b, int64_t now);
