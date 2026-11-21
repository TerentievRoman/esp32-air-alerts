#include "minitest.h"
#include "alert_log_buf.h"

#define DAY 86400
static alert_log_entry_t E(int64_t ts, alert_state_t st) { return (alert_log_entry_t){ ts, (uint8_t)st, ALERT_SRC_MQTT }; }

TEST(add_appends) {
    alert_log_buf_t b = {0};
    ASSERT_TRUE(alert_log_buf_add(&b, E(1000, ALERT_STATE_RED), 1000));
    ASSERT_EQ_INT(b.count, 1);
}
TEST(unknown_not_logged) {
    alert_log_buf_t b = {0};
    ASSERT_TRUE(!alert_log_buf_add(&b, E(1, ALERT_STATE_UNKNOWN), 1));
    ASSERT_EQ_INT(b.count, 0);
}
TEST(duplicate_skipped) {
    alert_log_buf_t b = {0};
    ASSERT_TRUE(alert_log_buf_add(&b, E(1000, ALERT_STATE_CLEAR), 1000));
    ASSERT_TRUE(!alert_log_buf_add(&b, E(1000, ALERT_STATE_CLEAR), 1000));
    ASSERT_EQ_INT(b.count, 1);
}
TEST(same_state_older_ts_skipped) {
    /* retained CLEAR replayed at boot is older than the CLEAR already logged */
    alert_log_buf_t b = {0};
    alert_log_buf_add(&b, E(5000, ALERT_STATE_CLEAR), 5000);
    ASSERT_TRUE(!alert_log_buf_add(&b, E(4000, ALERT_STATE_CLEAR), 6000));
    ASSERT_EQ_INT(b.count, 1);
}
TEST(same_state_new_ts_logged) {
    alert_log_buf_t b = {0};
    alert_log_buf_add(&b, E(1000, ALERT_STATE_RED), 1000);
    ASSERT_TRUE(alert_log_buf_add(&b, E(2000, ALERT_STATE_RED), 2000));
    ASSERT_EQ_INT(b.count, 2);
}
TEST(prunes_older_than_week) {
    alert_log_buf_t b = {0};
    alert_log_buf_add(&b, E(1000, ALERT_STATE_RED), 1000);
    alert_log_buf_add(&b, E(2000, ALERT_STATE_CLEAR), 2000);
    int64_t t = 1000 + 7 * DAY + 1;
    alert_log_buf_add(&b, E(t, ALERT_STATE_RED), t);
    ASSERT_EQ_INT(b.count, 2);
    ASSERT_EQ_INT(b.e[0].ts, 2000);
}
TEST(prune_uses_max_ts_when_clock_unsynced) {
    alert_log_buf_t b = {0};
    alert_log_buf_add(&b, E(1000, ALERT_STATE_RED), 0);
    alert_log_buf_add(&b, E(1000 + 8 * DAY, ALERT_STATE_CLEAR), 0);
    alert_log_buf_prune(&b, 0);
    ASSERT_EQ_INT(b.count, 1);
    ASSERT_EQ_INT(b.e[0].ts, 1000 + 8 * DAY);
}
TEST(zero_ts_kept) {
    alert_log_buf_t b = {0};
    alert_log_buf_add(&b, E(0, ALERT_STATE_RED), 0);
    alert_log_buf_add(&b, E(100 * DAY, ALERT_STATE_CLEAR), 100 * DAY);
    alert_log_buf_prune(&b, 100 * DAY);
    ASSERT_EQ_INT(b.count, 2);
}
TEST(overflow_drops_oldest) {
    alert_log_buf_t b = {0};
    for (int i = 1; i <= ALERT_LOG_CAP + 1; i++)
        alert_log_buf_add(&b, E(i, i % 2 ? ALERT_STATE_RED : ALERT_STATE_CLEAR), ALERT_LOG_CAP + 1);
    ASSERT_EQ_INT(b.count, ALERT_LOG_CAP);
    ASSERT_EQ_INT(b.e[0].ts, 2);
    ASSERT_EQ_INT(b.e[ALERT_LOG_CAP - 1].ts, ALERT_LOG_CAP + 1);
}

int main(void) {
    RUN(add_appends); RUN(unknown_not_logged); RUN(duplicate_skipped); RUN(same_state_older_ts_skipped); RUN(same_state_new_ts_logged);
    RUN(prunes_older_than_week); RUN(prune_uses_max_ts_when_clock_unsynced); RUN(zero_ts_kept);
    RUN(overflow_drops_oldest);
    return MT_RESULT();
}
