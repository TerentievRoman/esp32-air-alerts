#include "silence.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "alert_state.h"
#include "clock.h"
#include "notify.h"

static const char *TAG = "silence";

#define SILENCE_HOUR  9
#define SILENCE_MIN   0
#define SILENCE_TRACK 3
/* esp_timer may fire a little early; look this far ahead so we never re-arm
 * for the same 09:00 we are firing for. */
#define EARLY_FIRE_SLACK_S 2

static esp_timer_handle_t s_timer;

static void fire(void *arg);

static bool ensure_timer(void)
{
    if (s_timer) return true;
    const esp_timer_create_args_t a = { .callback = fire, .name = "silence" };
    return esp_timer_create(&a, &s_timer) == ESP_OK;
}

void silence_reschedule(void)
{
#if CONFIG_AA_SILENCE_ENABLE
    int64_t now = clock_now();
    if (now == 0 || !ensure_timer()) return;
    int64_t wait = clock_seconds_until_daily((time_t)(now + EARLY_FIRE_SLACK_S),
                                             SILENCE_HOUR, SILENCE_MIN) + EARLY_FIRE_SLACK_S;
    esp_timer_stop(s_timer);
    esp_timer_start_once(s_timer, (uint64_t)wait * 1000000ULL);
    ESP_LOGI(TAG, "next minute of silence in %lld s", (long long)wait);
#endif
}

static void fire(void *arg)
{
    (void)arg;
    if (alert_state_get() == ALERT_STATE_RED) {
        ESP_LOGW(TAG, "09:00 during RED: skipped");
    } else {
        ESP_LOGI(TAG, "09:00: minute of silence");
        notify_play(SILENCE_TRACK);
    }
    silence_reschedule();
}

void silence_init(void)
{
#if CONFIG_AA_SILENCE_ENABLE
    ensure_timer();
    silence_reschedule();
#else
    ESP_LOGI(TAG, "disabled");
#endif
}
