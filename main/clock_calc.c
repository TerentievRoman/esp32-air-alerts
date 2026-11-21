/* Pure time arithmetic (no ESP headers) so it can be unit-tested on the host. */
#include "clock.h"

int64_t clock_seconds_until_daily(time_t now, int hour, int min)
{
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    time_t target = mktime(&t);
    if (target <= now) {
        localtime_r(&now, &t);
        t.tm_mday += 1;
        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_sec = 0;
        t.tm_isdst = -1;
        target = mktime(&t);
    }
    return (int64_t)(target - now);
}
