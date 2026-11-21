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

int main(void) {
    RUN(boot_into_clear_is_silent); RUN(real_clear_plays_track2); RUN(red_plays_siren);
    RUN(stale_red_silent); RUN(yellow_plays_siren); RUN(stale_yellow_silent); RUN(to_unknown_silent);
    return MT_RESULT();
}
