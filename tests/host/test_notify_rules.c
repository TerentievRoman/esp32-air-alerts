#include "minitest.h"
#include "notify_rules.h"

TEST(boot_into_clear_is_silent) {
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_UNKNOWN, ALERT_STATE_CLEAR, false), 0);
}
TEST(real_clear_plays_track2) {
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_RED, ALERT_STATE_CLEAR, false), 2);
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_YELLOW, ALERT_STATE_CLEAR, false), 2);
}
TEST(red_plays_siren) {
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_CLEAR, ALERT_STATE_RED, false), 1);
}
TEST(stale_red_silent) {
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_UNKNOWN, ALERT_STATE_RED, true), 0);
}
TEST(yellow_plays_siren) {
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_CLEAR, ALERT_STATE_YELLOW, false), 1);
}
TEST(stale_yellow_silent) {
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_UNKNOWN, ALERT_STATE_YELLOW, true), 0);
}
TEST(to_unknown_silent) {
    ASSERT_EQ_INT(notify_track_for(ALERT_STATE_RED, ALERT_STATE_UNKNOWN, false), 0);
}

TEST(any_gesture_while_playing_only_stops) {
    ASSERT_EQ_INT(notify_button_action(BTN_STOP, true), BTN_ACTION_STOP);
    ASSERT_EQ_INT(notify_button_action(BTN_ALERT, true), BTN_ACTION_STOP);
    ASSERT_EQ_INT(notify_button_action(BTN_CLEAR, true), BTN_ACTION_STOP);
}
TEST(gestures_when_silent) {
    ASSERT_EQ_INT(notify_button_action(BTN_STOP, false), BTN_ACTION_NONE);
    ASSERT_EQ_INT(notify_button_action(BTN_ALERT, false), BTN_ACTION_ALERT);
    ASSERT_EQ_INT(notify_button_action(BTN_CLEAR, false), BTN_ACTION_CLEAR);
}

TEST(alert_interrupted_only_by_clear) {
    ASSERT_TRUE(notify_can_preempt(1, 2));
    ASSERT_TRUE(!notify_can_preempt(1, 1));
    ASSERT_TRUE(!notify_can_preempt(1, 3));
}
TEST(clear_interrupted_only_by_alert) {
    ASSERT_TRUE(notify_can_preempt(2, 1));
    ASSERT_TRUE(!notify_can_preempt(2, 2));
    ASSERT_TRUE(!notify_can_preempt(2, 3));
}
TEST(anthem_interrupted_only_by_alert) {
    ASSERT_TRUE(notify_can_preempt(3, 1));
    ASSERT_TRUE(!notify_can_preempt(3, 2));
    ASSERT_TRUE(!notify_can_preempt(3, 3));
}

TEST(clear_during_anthem_is_deferred) {
    ASSERT_EQ_INT(notify_sound_decision(3, 2), SOUND_DEFER);
}
TEST(anthem_during_alert_is_deferred) {
    ASSERT_EQ_INT(notify_sound_decision(1, 3), SOUND_DEFER);
}
TEST(alert_during_anthem_interrupts) {
    ASSERT_EQ_INT(notify_sound_decision(3, 1), SOUND_INTERRUPT);
}
TEST(same_track_is_ignored) {
    ASSERT_EQ_INT(notify_sound_decision(1, 1), SOUND_IGNORE);
    ASSERT_EQ_INT(notify_sound_decision(3, 3), SOUND_IGNORE);
}

int main(void) {
    RUN(boot_into_clear_is_silent); RUN(real_clear_plays_track2); RUN(red_plays_siren);
    RUN(stale_red_silent); RUN(yellow_plays_siren); RUN(stale_yellow_silent); RUN(to_unknown_silent);
    RUN(any_gesture_while_playing_only_stops); RUN(gestures_when_silent);
    RUN(alert_interrupted_only_by_clear); RUN(clear_interrupted_only_by_alert); RUN(anthem_interrupted_only_by_alert);
    RUN(clear_during_anthem_is_deferred); RUN(anthem_during_alert_is_deferred);
    RUN(alert_during_anthem_interrupts); RUN(same_track_is_ignored);
    return MT_RESULT();
}
