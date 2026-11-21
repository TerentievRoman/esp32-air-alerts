#pragma once
/* BOOT button gestures from the debounced level: pure logic, host-tested. */
#include <stdbool.h>
#include <stdint.h>

#define GESTURE_DOUBLE_MS 400   /* max gap between two short presses */
#define GESTURE_LONG_MS   2000  /* hold time for a long press */

typedef enum {
    GESTURE_NONE = 0,
    GESTURE_SHORT,   /* one short press, reported after the double window */
    GESTURE_DOUBLE,  /* two short presses */
    GESTURE_LONG,    /* reported while still held, once */
} gesture_t;

typedef struct {
    bool down;
    bool long_fired;
    bool pending_short;
    bool ignore;      /* button was down at start: wait for release */
    uint32_t down_at;
    uint32_t release_at;
} gesture_fsm_t;

/* Optional: start with the current level (a button held at start is ignored
 * until released). A zero-initialised FSM assumes "released". */
void gesture_init(gesture_fsm_t *g, bool pressed, uint32_t now_ms);

/* Call on every poll with the debounced level. */
gesture_t gesture_feed(gesture_fsm_t *g, bool pressed, uint32_t now_ms);
