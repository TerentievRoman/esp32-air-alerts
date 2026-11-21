#include "alert_log_buf.h"

#include <string.h>

void alert_log_buf_prune(alert_log_buf_t *b, int64_t now)
{
    int64_t ref = now;
    if (ref <= 0) {
        for (int i = 0; i < b->count; i++) {
            if (b->e[i].ts > ref) ref = b->e[i].ts;
        }
    }
    if (ref <= 0) return;

    int w = 0;
    for (int i = 0; i < b->count; i++) {
        const alert_log_entry_t *x = &b->e[i];
        if (x->ts > 0 && ref - x->ts > ALERT_LOG_MAX_AGE_SEC) continue;
        b->e[w++] = *x;
    }
    b->count = (uint16_t)w;
}

bool alert_log_buf_add(alert_log_buf_t *b, alert_log_entry_t x, int64_t now)
{
    if (x.state == ALERT_STATE_UNKNOWN) return false;
    if (b->count > 0) {
        const alert_log_entry_t *last = &b->e[b->count - 1];
        /* Same state, not newer: a replay (e.g. retained message after reboot). */
        if (last->state == x.state && x.ts <= last->ts) return false;
    }
    alert_log_buf_prune(b, now);
    if (b->count == ALERT_LOG_CAP) {
        memmove(&b->e[0], &b->e[1], (ALERT_LOG_CAP - 1) * sizeof(b->e[0]));
        b->count--;
    }
    b->e[b->count++] = x;
    return true;
}
