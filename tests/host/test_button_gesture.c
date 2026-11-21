#include "minitest.h"
#include "button_gesture.h"

/* Feed a constant debounced level from t0 to t1 in 10 ms steps; return the
 * first non-NONE gesture seen (or NONE). */
static gesture_t run(gesture_fsm_t *g, bool pressed, uint32_t t0, uint32_t t1)
{
    gesture_t first = GESTURE_NONE;
    for (uint32_t t = t0; t < t1; t += 10) {
        gesture_t e = gesture_feed(g, pressed, t);
        if (e != GESTURE_NONE && first == GESTURE_NONE) first = e;
    }
    return first;
}

TEST(short_press_is_short_after_double_window) {
    gesture_fsm_t g = {0};
    ASSERT_EQ_INT(run(&g, true, 0, 100), GESTURE_NONE);
    ASSERT_EQ_INT(run(&g, false, 100, 100 + GESTURE_DOUBLE_MS - 20), GESTURE_NONE);
    ASSERT_EQ_INT(run(&g, false, 100 + GESTURE_DOUBLE_MS - 20, 1000), GESTURE_SHORT);
}
TEST(double_press) {
    gesture_fsm_t g = {0};
    run(&g, true, 0, 100);
    run(&g, false, 100, 250);
    ASSERT_EQ_INT(run(&g, true, 250, 350), GESTURE_NONE);
    ASSERT_EQ_INT(run(&g, false, 350, 360), GESTURE_DOUBLE);
    /* nothing more afterwards */
    ASSERT_EQ_INT(run(&g, false, 360, 2000), GESTURE_NONE);
}
TEST(long_press_fires_while_held) {
    gesture_fsm_t g = {0};
    ASSERT_EQ_INT(run(&g, true, 0, GESTURE_LONG_MS - 10), GESTURE_NONE);
    ASSERT_EQ_INT(run(&g, true, GESTURE_LONG_MS - 10, GESTURE_LONG_MS + 20), GESTURE_LONG);
    /* keep holding, then release: no extra gesture */
    ASSERT_EQ_INT(run(&g, true, GESTURE_LONG_MS + 20, 5000), GESTURE_NONE);
    ASSERT_EQ_INT(run(&g, false, 5000, 6000), GESTURE_NONE);
}
TEST(slow_second_press_is_two_shorts) {
    gesture_fsm_t g = {0};
    run(&g, true, 0, 100);
    ASSERT_EQ_INT(run(&g, false, 100, 900), GESTURE_SHORT);
    run(&g, true, 900, 1000);
    ASSERT_EQ_INT(run(&g, false, 1000, 2000), GESTURE_SHORT);
}
TEST(medium_hold_is_short) {
    /* held 1 s (< long): still a short press */
    gesture_fsm_t g = {0};
    run(&g, true, 0, 1000);
    ASSERT_EQ_INT(run(&g, false, 1000, 2000), GESTURE_SHORT);
}
TEST(held_at_start_ignored_until_release) {
    /* button already down when the FSM starts (e.g. held through boot reset) */
    gesture_fsm_t g = {0};
    gesture_init(&g, true, 0);
    ASSERT_EQ_INT(run(&g, true, 0, 6000), GESTURE_NONE);
    ASSERT_EQ_INT(run(&g, false, 6000, 7000), GESTURE_NONE);
}

int main(void) {
    RUN(short_press_is_short_after_double_window); RUN(double_press); RUN(long_press_fires_while_held);
    RUN(slow_second_press_is_two_shorts); RUN(medium_hold_is_short); RUN(held_at_start_ignored_until_release);
    return MT_RESULT();
}
