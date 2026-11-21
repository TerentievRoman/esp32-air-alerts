#include "screen_fmt.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "font5x8.h"

void utf8_truncate(char *s, int max_chars)
{
    const char *p = s;
    for (int n = 0; n < max_chars; n++) {
        if (utf8_next(&p) == 0) return;
    }
    s[p - s] = '\0';
}

static int utf8_len(const char *s)
{
    int n = 0;
    while (utf8_next(&s)) n++;
    return n;
}

static void put(screen_lines_t *o, int row, const char *text)
{
    strlcpy(o->text[row], text, SCREEN_LINE_BYTES);
    utf8_truncate(o->text[row], SCREEN_COLS);
}

static void put_centered(screen_lines_t *o, int row, const char *text)
{
    int pad = (SCREEN_COLS - utf8_len(text)) / 2;
    if (pad < 0) pad = 0;
    char buf[SCREEN_LINE_BYTES];
    snprintf(buf, sizeof(buf), "%*s%s", pad, "", text);
    put(o, row, buf);
}

static void put_time(screen_lines_t *o, int row, int64_t ts, const char *fmt)
{
    time_t t = (time_t)ts;
    struct tm tm;
    localtime_r(&t, &tm);
    char buf[SCREEN_LINE_BYTES];
    strftime(buf, sizeof(buf), fmt, &tm);
    put(o, row, buf);
}

static const char *state_upper(alert_state_t s)
{
    switch (s) {
    case ALERT_STATE_RED:    return "ТРИВОГА";
    case ALERT_STATE_YELLOW: return "ДРОНИ";
    case ALERT_STATE_CLEAR:  return "ВІДБІЙ";
    default:                 return "НЕВІДОМО";
    }
}

static const char *state_title(alert_state_t s)
{
    switch (s) {
    case ALERT_STATE_RED:    return "Тривога";
    case ALERT_STATE_YELLOW: return "Дрони";
    case ALERT_STATE_CLEAR:  return "Відбій";
    default:                 return "Невідомо";
    }
}

static void format_ap(const screen_info_t *in, screen_lines_t *o)
{
    put(o, 0, "Налаштування");
    put(o, 1, "Мережа:");
    put(o, 2, in->ap_ssid);
    /* rest of the SSID that did not fit on row 2 */
    const char *p = in->ap_ssid;
    for (int n = 0; n < SCREEN_COLS && utf8_next(&p); n++) {}
    put(o, 3, p);
    put(o, 4, "Відкрийте");
    put(o, 5, "192.168.4.1");
}

void screen_format(const screen_info_t *in, screen_lines_t *o)
{
    memset(o, 0, sizeof(*o));
    if (in->ap_mode) {
        format_ap(in, o);
        return;
    }

    if (in->now > 0) put_time(o, 0, in->now, "%H:%M   %d.%m");
    else put(o, 0, "--:--");

    put_centered(o, 1, state_upper(in->state));
    o->invert[1] = in->state == ALERT_STATE_RED || in->state == ALERT_STATE_YELLOW;

    char buf[SCREEN_LINE_BYTES];
    if (in->wifi) {
        snprintf(buf, sizeof(buf), "WiFi %s", in->ssid);
        put(o, 2, buf);
    } else {
        put(o, 2, "WiFi немає");
    }

    put(o, 3, in->mqtt ? "MQTT ок" : "MQTT немає");

    if (!in->has_log) {
        put(o, 4, "Журнал");
        put(o, 5, "порожній");
        return;
    }
    snprintf(buf, sizeof(buf), "Ост: %s", state_title((alert_state_t)in->last.state));
    put(o, 4, buf);
    if (in->last.ts > 0) {
        put_time(o, 5, in->last.ts,
                 in->last.src == ALERT_SRC_LOCAL ? "%d.%m %H:%M кн" : "%d.%m %H:%M бр");
    } else {
        put(o, 5, "час невідомий");
    }
}
