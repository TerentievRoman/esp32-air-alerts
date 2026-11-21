#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* SNTP (pool.ntp.org) + TZ Europe/Kyiv. on_sync is called after every sync. */
void clock_init(void (*on_sync)(void));

bool clock_is_synced(void);

/* Unix time, or 0 while the clock is not synced. */
int64_t clock_now(void);

/* Seconds until the next local hour:min:00, always > 0
 * (exactly at hour:min:00 the answer is the next day). Pure, in clock_calc.c. */
int64_t clock_seconds_until_daily(time_t now, int hour, int min);
