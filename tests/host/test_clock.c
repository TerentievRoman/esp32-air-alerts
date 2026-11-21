#include <stdlib.h>
#include <time.h>
#include "minitest.h"
#include "clock.h"

/* Local Kyiv time -> time_t. */
static time_t kyiv(int y, int mo, int d, int h, int mi, int s)
{
    struct tm t = { .tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d,
                    .tm_hour = h, .tm_min = mi, .tm_sec = s, .tm_isdst = -1 };
    return mktime(&t);
}

TEST(before_nine) {
    ASSERT_EQ_INT(clock_seconds_until_daily(kyiv(2026, 11, 21, 8, 59, 30), 9, 0), 30);
}
TEST(exactly_nine_gives_tomorrow) {
    ASSERT_EQ_INT(clock_seconds_until_daily(kyiv(2026, 11, 21, 9, 0, 0), 9, 0), 86400);
}
TEST(after_nine_gives_tomorrow) {
    ASSERT_EQ_INT(clock_seconds_until_daily(kyiv(2026, 11, 21, 9, 0, 1), 9, 0), 86399);
}
TEST(late_evening) {
    ASSERT_EQ_INT(clock_seconds_until_daily(kyiv(2026, 11, 21, 23, 0, 0), 9, 0), 10 * 3600);
}
TEST(dst_spring_forward_night) {
    /* 2026-03-29 03:00 -> 04:00: night is one hour shorter */
    ASSERT_EQ_INT(clock_seconds_until_daily(kyiv(2026, 3, 28, 23, 0, 0), 9, 0), 9 * 3600);
}
TEST(dst_fall_back_night) {
    /* 2026-10-25 04:00 -> 03:00: night is one hour longer */
    ASSERT_EQ_INT(clock_seconds_until_daily(kyiv(2026, 10, 24, 23, 0, 0), 9, 0), 11 * 3600);
}
TEST(kyiv_nine_is_utc_seven_in_winter) {
    /* 2026-11-21 07:00:00 UTC == 09:00 Kyiv (EET, UTC+2) */
    time_t utc_0659 = 1795244340; /* 2026-11-21 06:59:00 UTC */
    ASSERT_EQ_INT(clock_seconds_until_daily(utc_0659, 9, 0), 60);
}

int main(void) {
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();
    RUN(before_nine); RUN(exactly_nine_gives_tomorrow); RUN(after_nine_gives_tomorrow);
    RUN(late_evening); RUN(dst_spring_forward_night); RUN(dst_fall_back_night);
    RUN(kyiv_nine_is_utc_seven_in_winter);
    return MT_RESULT();
}
