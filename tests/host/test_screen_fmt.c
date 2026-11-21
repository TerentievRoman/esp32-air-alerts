#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "minitest.h"
#include "screen_fmt.h"

#define T0 1795299720  /* 2026-11-22 00:22:00 Kyiv */

TEST(time_line)        { screen_info_t i = {.now = T0}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[0], "00:22   22.11"); }
TEST(time_unsynced)    { screen_info_t i = {0}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[0], "--:--"); }
TEST(state_red_inverted) { screen_info_t i = {.state = ALERT_STATE_RED}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[1], "   ТРИВОГА"); ASSERT_TRUE(o.invert[1]); }
TEST(state_yellow_inverted) { screen_info_t i = {.state = ALERT_STATE_YELLOW}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[1], "    ДРОНИ"); ASSERT_TRUE(o.invert[1]); }
TEST(state_clear_plain)  { screen_info_t i = {.state = ALERT_STATE_CLEAR}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[1], "    ВІДБІЙ"); ASSERT_TRUE(!o.invert[1]); }
TEST(wifi_long_ssid_cut_on_char) { screen_info_t i = {.wifi = true}; strcpy(i.ssid, "Моя_Домашня_Мережа"); screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[2], "WiFi Моя_Домаш"); }
TEST(wifi_none)        { screen_info_t i = {0}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[2], "WiFi немає"); }
TEST(mqtt_lines)       { screen_info_t i = {.mqtt = true}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[3], "MQTT ок");
                         i.mqtt = false; screen_format(&i, &o); ASSERT_EQ_STR(o.text[3], "MQTT немає"); }
TEST(log_empty)        { screen_info_t i = {0}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[4], "Журнал"); ASSERT_EQ_STR(o.text[5], "порожній"); }
TEST(log_last_red_local) { screen_info_t i = {.has_log = true, .last = {T0 - 60, ALERT_STATE_RED, ALERT_SRC_LOCAL}}; screen_lines_t o; screen_format(&i, &o);
                           ASSERT_EQ_STR(o.text[4], "Ост: Тривога"); ASSERT_EQ_STR(o.text[5], "22.11 00:21 кн"); }
TEST(log_last_clear_mqtt) { screen_info_t i = {.has_log = true, .last = {T0, ALERT_STATE_CLEAR, ALERT_SRC_MQTT}}; screen_lines_t o; screen_format(&i, &o);
                            ASSERT_EQ_STR(o.text[4], "Ост: Відбій"); ASSERT_EQ_STR(o.text[5], "22.11 00:22 бр"); }
TEST(log_ts_unknown)   { screen_info_t i = {.has_log = true, .last = {0, ALERT_STATE_CLEAR, 0}}; screen_lines_t o; screen_format(&i, &o); ASSERT_EQ_STR(o.text[5], "час невідомий"); }
TEST(ap_mode)          { screen_info_t i = {.ap_mode = true}; strcpy(i.ap_ssid, "air-alert-bdfc40"); screen_lines_t o; screen_format(&i, &o);
                         ASSERT_EQ_STR(o.text[0], "Налаштування"); ASSERT_EQ_STR(o.text[1], "Мережа:");
                         ASSERT_EQ_STR(o.text[2], "air-alert-bdfc"); ASSERT_EQ_STR(o.text[3], "40");
                         ASSERT_EQ_STR(o.text[4], "Відкрийте"); ASSERT_EQ_STR(o.text[5], "192.168.4.1");
                         for (int r = 0; r < SCREEN_ROWS; r++) ASSERT_TRUE(!o.invert[r]); }
TEST(truncate_emoji_boundary) { char s[32] = "ab\xF0\x9F\x98\x80" "cd"; utf8_truncate(s, 3); ASSERT_EQ_STR(s, "ab\xF0\x9F\x98\x80"); }
TEST(truncate_short_unchanged) { char s[8] = "abc"; utf8_truncate(s, 14); ASSERT_EQ_STR(s, "abc"); }

int main(void) {
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();
    RUN(time_line); RUN(time_unsynced); RUN(state_red_inverted); RUN(state_yellow_inverted); RUN(state_clear_plain);
    RUN(wifi_long_ssid_cut_on_char); RUN(wifi_none); RUN(mqtt_lines); RUN(log_empty); RUN(log_last_red_local);
    RUN(log_last_clear_mqtt); RUN(log_ts_unknown); RUN(ap_mode); RUN(truncate_emoji_boundary); RUN(truncate_short_unchanged);
    return MT_RESULT();
}
