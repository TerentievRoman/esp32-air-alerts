#pragma once
/* LCD content as 6 text lines (pure, host-tested). */
#include <stdbool.h>
#include <stdint.h>
#include "alert_log_buf.h"
#include "alert_state.h"

#define SCREEN_ROWS       6
#define SCREEN_COLS       14
#define SCREEN_LINE_BYTES 64

typedef struct {
    bool ap_mode;
    char ap_ssid[33];
    int64_t now;            /* unix time, 0 = not synced */
    alert_state_t state;
    bool wifi;
    char ssid[33];
    bool mqtt;
    bool has_log;
    alert_log_entry_t last;
} screen_info_t;

typedef struct {
    char text[SCREEN_ROWS][SCREEN_LINE_BYTES];
    bool invert[SCREEN_ROWS];
} screen_lines_t;

/* Local time via localtime_r (TZ must be set by the caller). */
void screen_format(const screen_info_t *in, screen_lines_t *out);

/* Cut s after max_chars UTF-8 characters (never inside a character). */
void utf8_truncate(char *s, int max_chars);
