#include "button_gesture.h"

void gesture_init(gesture_fsm_t *g, bool pressed, uint32_t now_ms)
{
    *g = (gesture_fsm_t){ .ignore = pressed, .down_at = now_ms };
}

gesture_t gesture_feed(gesture_fsm_t *g, bool pressed, uint32_t now_ms)
{
    if (g->ignore) {
        if (!pressed) g->ignore = false;
        return GESTURE_NONE;
    }

    if (pressed) {
        if (!g->down) {
            g->down = true;
            g->down_at = now_ms;
            g->long_fired = false;
            return GESTURE_NONE;
        }
        if (!g->long_fired && now_ms - g->down_at >= GESTURE_LONG_MS) {
            g->long_fired = true;
            g->pending_short = false;
            return GESTURE_LONG;
        }
        return GESTURE_NONE;
    }

    if (g->down) {                       /* release */
        g->down = false;
        if (g->long_fired) return GESTURE_NONE;
        if (g->pending_short) {
            g->pending_short = false;
            return GESTURE_DOUBLE;
        }
        g->pending_short = true;
        g->release_at = now_ms;
        return GESTURE_NONE;
    }

    if (g->pending_short && now_ms - g->release_at >= GESTURE_DOUBLE_MS) {
        g->pending_short = false;
        return GESTURE_SHORT;
    }
    return GESTURE_NONE;
}
