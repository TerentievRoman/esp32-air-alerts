#include "minitest.h"
#include "alert_state.h"

TEST(parse_red) {
    alert_msg_t m;
    ASSERT_TRUE(alert_state_parse("{\"state\":\"RED\",\"ts\":1700000000,\"src\":\"x\"}", 0, &m));
    ASSERT_EQ_INT(m.state, ALERT_STATE_RED);
    ASSERT_TRUE(m.ts == 1700000000);
}
TEST(parse_yellow_and_clear) {
    alert_msg_t m;
    ASSERT_TRUE(alert_state_parse("{\"state\":\"YELLOW\",\"ts\":1}", 0, &m));
    ASSERT_EQ_INT(m.state, ALERT_STATE_YELLOW);
    ASSERT_TRUE(alert_state_parse("{\"state\":\"CLEAR\",\"ts\":2}", 0, &m));
    ASSERT_EQ_INT(m.state, ALERT_STATE_CLEAR);
}
TEST(parse_respects_len) {
    const char *buf = "{\"state\":\"RED\",\"ts\":7}garbage";
    alert_msg_t m;
    ASSERT_TRUE(alert_state_parse(buf, 23, &m));
    ASSERT_EQ_INT(m.ts, 7);
}
TEST(parse_unknown_state_fails) { alert_msg_t m; ASSERT_TRUE(!alert_state_parse("{\"state\":\"PURPLE\",\"ts\":1}", 0, &m)); }
TEST(parse_missing_ts_fails) { alert_msg_t m; ASSERT_TRUE(!alert_state_parse("{\"state\":\"RED\"}", 0, &m)); }
TEST(parse_string_ts_fails) { alert_msg_t m; ASSERT_TRUE(!alert_state_parse("{\"state\":\"RED\",\"ts\":\"5\"}", 0, &m)); }
TEST(parse_garbage_fails) { alert_msg_t m; ASSERT_TRUE(!alert_state_parse("not json", 0, &m)); }
TEST(stale_when_old) { alert_msg_t m = {ALERT_STATE_RED, 1000, ALERT_SRC_MQTT}; ASSERT_TRUE(alert_state_is_stale(&m, 1000 + 10801, 10800)); }
TEST(fresh_when_recent) { alert_msg_t m = {ALERT_STATE_RED, 1000, ALERT_SRC_MQTT}; ASSERT_TRUE(!alert_state_is_stale(&m, 1000 + 10800, 10800)); }
TEST(fresh_when_clock_unsynced) { alert_msg_t m = {ALERT_STATE_RED, 1, ALERT_SRC_MQTT}; ASSERT_TRUE(!alert_state_is_stale(&m, 0, 10800)); }

TEST(apply_same_state_returns_false) {
    alert_state_init();
    alert_msg_t m = {ALERT_STATE_RED, 5, ALERT_SRC_MQTT};
    ASSERT_TRUE(alert_state_apply(&m, 0));
    ASSERT_TRUE(!alert_state_apply(&m, 0));
}

static int cb_calls;
static alert_state_t cb_old, cb_new;
static bool cb_stale;
static void record_cb(alert_state_t o, alert_state_t n, bool stale) { cb_calls++; cb_old = o; cb_new = n; cb_stale = stale; }

TEST(apply_calls_callback_with_stale) {
    alert_state_init();
    cb_calls = 0;
    alert_state_set_callback(record_cb);
    alert_msg_t m = {ALERT_STATE_RED, 1000, ALERT_SRC_MQTT};
    ASSERT_TRUE(alert_state_apply(&m, 1000 + 20000));
    ASSERT_EQ_INT(cb_calls, 1);
    ASSERT_EQ_INT(cb_old, ALERT_STATE_UNKNOWN);
    ASSERT_EQ_INT(cb_new, ALERT_STATE_RED);
    ASSERT_TRUE(cb_stale);
    alert_state_set_callback(NULL);
}
TEST(apply_no_callback_when_unchanged) {
    alert_state_init();
    alert_msg_t m = {ALERT_STATE_CLEAR, 1, ALERT_SRC_MQTT};
    alert_state_apply(&m, 0);
    cb_calls = 0;
    alert_state_set_callback(record_cb);
    ASSERT_TRUE(!alert_state_apply(&m, 0));
    ASSERT_EQ_INT(cb_calls, 0);
    alert_state_set_callback(NULL);
}
TEST(last_msg_tracks_applied_message) {
    alert_state_init();
    alert_msg_t m = {ALERT_STATE_RED, 1234, 1};
    alert_state_apply(&m, 0);
    alert_msg_t l = alert_state_last_msg();
    ASSERT_EQ_INT(l.state, ALERT_STATE_RED);
    ASSERT_EQ_INT(l.ts, 1234);
    ASSERT_EQ_INT(l.src, 1);
}
TEST(parse_sets_src_mqtt) {
    alert_msg_t m; m.src = 7;
    ASSERT_TRUE(alert_state_parse("{\"state\":\"RED\",\"ts\":1}", 0, &m));
    ASSERT_EQ_INT(m.src, 0);
}
TEST(names) {
    ASSERT_EQ_STR(alert_state_name(ALERT_STATE_UNKNOWN), "UNKNOWN");
    ASSERT_EQ_STR(alert_state_name(ALERT_STATE_CLEAR), "CLEAR");
    ASSERT_EQ_STR(alert_state_name(ALERT_STATE_YELLOW), "YELLOW");
    ASSERT_EQ_STR(alert_state_name(ALERT_STATE_RED), "RED");
}

int main(void) {
    RUN(parse_red); RUN(parse_yellow_and_clear); RUN(parse_respects_len);
    RUN(parse_unknown_state_fails); RUN(parse_missing_ts_fails); RUN(parse_string_ts_fails);
    RUN(parse_garbage_fails); RUN(stale_when_old); RUN(fresh_when_recent);
    RUN(fresh_when_clock_unsynced); RUN(apply_same_state_returns_false);
    RUN(apply_calls_callback_with_stale); RUN(apply_no_callback_when_unchanged); RUN(last_msg_tracks_applied_message); RUN(parse_sets_src_mqtt); RUN(names);
    return MT_RESULT();
}
