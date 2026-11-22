#include "notify.h"

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "alert_state.h"
#include "button.h"
#include "clock.h"
#include "dfplayer.h"
#include "led.h"
#include "mqtt.h"
#include "notify_rules.h"
#include "volume.h"
#include "alert_log.h"

static const char *TAG = "notify";

#define TRACK_MAX     3
#define POLL_MS       200
/* DFPlayer may not report track end (e.g. its TX line is not wired): cap each play. */
#define TRACK_MAX_MS  (6 * 60 * 1000)
/* Volume test: play this long, then stop. */
#define TEST_PLAY_MS  10000

typedef enum { CMD_PLAY, CMD_STOP, CMD_TRANSITION, CMD_BUTTON, CMD_APPLY, CMD_TEST } cmd_type_t;

typedef struct {
    cmd_type_t type;
    int track;              /* CMD_PLAY */
    int repeats;            /* CMD_PLAY */
    int vol;                /* CMD_TEST */
    alert_state_t old_state; /* CMD_TRANSITION */
    alert_state_t state;    /* CMD_TRANSITION */
    bool stale;             /* CMD_TRANSITION */
    int64_t ts;             /* CMD_TRANSITION: event time from the message */
    alert_src_t src;        /* CMD_TRANSITION */
    button_id_t button;     /* CMD_BUTTON */
    alert_msg_t msg;        /* CMD_APPLY */
} cmd_t;

static QueueHandle_t s_q;
static led_pattern_t s_base = LED_OFF; /* LED pattern of the current alert state */
static bool s_broker_lost;
static volatile bool s_stop_req;
static volatile bool s_playing;
static int s_cur_vol = -1;   /* last volume sent to DFPlayer */

/* --- producers (any context) --- */

static void post(const cmd_t *c)
{
    if (!s_q || xQueueSend(s_q, c, 0) != pdTRUE) ESP_LOGW(TAG, "queue full, cmd %d dropped", c->type);
}

void notify_play(int track)
{
    if (track < 1 || track > TRACK_MAX) return;
    post(&(cmd_t){ .type = CMD_PLAY, .track = track, .repeats = 1 });
}

bool notify_test_volume(int track, int vol)
{
    if (track < 1 || track > TRACK_MAX || vol < 0 || vol > VOLUME_MAX) return false;
    if (s_playing) return false;
    post(&(cmd_t){ .type = CMD_TEST, .track = track, .repeats = 1, .vol = vol });
    return true;
}

void notify_apply(const alert_msg_t *m)
{
    post(&(cmd_t){ .type = CMD_APPLY, .msg = *m });
}

void notify_stop(void)
{
    s_stop_req = true;
    post(&(cmd_t){ .type = CMD_STOP });
}

static void on_state_change(alert_state_t old_state, alert_state_t new_state, bool stale)
{
    /* Called synchronously inside alert_state_apply: last_msg is this message. */
    const alert_msg_t m = alert_state_last_msg();
    post(&(cmd_t){ .type = CMD_TRANSITION, .old_state = old_state, .state = new_state,
                   .stale = stale, .ts = m.ts, .src = (alert_src_t)m.src });
}

/* Runs in the esp_timer task: only flag / enqueue. */
static void on_button(button_id_t id)
{
    switch (notify_button_action(id, s_playing)) {
    case BTN_ACTION_STOP:
        s_stop_req = true;
        break;
    case BTN_ACTION_ALERT:
    case BTN_ACTION_CLEAR:
        post(&(cmd_t){ .type = CMD_BUTTON, .button = id });
        break;
    default:
        break;
    }
}

/* --- notify task --- */

static led_pattern_t pattern_for(alert_state_t s)
{
    switch (s) {
    case ALERT_STATE_RED:    return LED_ON;
    case ALERT_STATE_YELLOW: return LED_BLINK_1HZ;
    default:                 return LED_OFF;
    }
}

static void handle_button(button_id_t id);
static void handle_apply(const alert_msg_t *m);
static int transition_effects(const cmd_t *c, int *repeats);

/* Sound that interrupted the previous one: played next by the task loop. */
static cmd_t s_next;
static bool s_have_next;

/* Play a track `repeats` times. Returns early on BOOT/stop, or when a new
 * sound may interrupt this one (notify_can_preempt); it is then queued in
 * s_next. Other commands are handled while playing.
 * test_vol >= 0: volume test at that volume (repeats must be 1): limited to
 * TEST_PLAY_MS, no schedule changes, any new sound interrupts it. */
static void play_body(int track, int repeats, int test_vol);

/* Send the volume to DFPlayer unconditionally. DFPlayer has no ACK, so the
 * cache may lie after a DFPlayer reset or a failed init. The cached value
 * changes only when the send succeeded. */
static void send_volume(int v)
{
    ESP_LOGI(TAG, "volume %d", v);
    if (dfplayer_set_volume(v) == ESP_OK) s_cur_vol = v;
    else ESP_LOGW(TAG, "set volume %d failed", v);
}

/* Send the volume only when it differs from the cached one (cheap periodic
 * check); a failure is retried on the next call. */
static void apply_volume(int v)
{
    if (v == s_cur_vol) return;
    send_volume(v);
}

static void play(int track, int repeats, int test_vol)
{
    s_playing = true;
    play_body(track, repeats, test_vol);
    if (test_vol >= 0) {
        /* Test over for any reason: silence it and go back to the schedule. */
        if (!s_have_next) {
            dfplayer_stop();
            apply_volume(volume_now());
        }
        /* else: an interrupting sound already stopped the player and sets its own volume */
    }
    s_playing = false;
}

static void play_body(int track, int repeats, int test_vol)
{
    const bool test = test_vol >= 0;
    const int limit_ms = test ? TEST_PLAY_MS : TRACK_MAX_MS;
    s_stop_req = false;
    cmd_t deferred = { 0 };
    bool has_deferred = false;   /* last sound that may not interrupt this one */
    for (int r = 0; r < repeats; r++) {
        ESP_LOGI(TAG, "play track %d (%d/%d)", track, r + 1, repeats);
        if (test) {
            ESP_LOGI(TAG, "volume test: %d", test_vol);
            if (dfplayer_set_volume(test_vol) == ESP_OK) s_cur_vol = test_vol;
            else {
                ESP_LOGW(TAG, "set volume %d failed", test_vol);
                s_cur_vol = -1;   /* unknown: force a re-send on restore */
            }
        } else {
            send_volume(volume_now());
        }
        dfplayer_play_track(track);
        int64_t minute = clock_now() / 60;
        for (int waited = 0; waited < limit_ms; waited += POLL_MS) {
            if (dfplayer_wait_finished(POLL_MS)) break;
            if (s_stop_req) {
                ESP_LOGI(TAG, "stopped by button%s", has_deferred ? ", deferred sound dropped" : "");
                dfplayer_stop();
                return;
            }
            int64_t m = clock_now() / 60;
            if (!test && m != minute) {
                minute = m;
                apply_volume(volume_now());
            }
            cmd_t next;
            if (xQueueReceive(s_q, &next, 0) != pdTRUE) continue;

            int next_track = 0, next_repeats = 1;
            cmd_t follow = { 0 };   /* what s_next gets if this sound is interrupted */
            switch (next.type) {
            case CMD_BUTTON: handle_button(next.button); continue;
            case CMD_APPLY:  handle_apply(&next.msg);    continue;
            case CMD_STOP:
                dfplayer_stop();
                return;
            case CMD_TRANSITION:
                /* State, LED and log always follow; only the sound is subject
                 * to the interruption rules. */
                next_track = transition_effects(&next, &next_repeats);
                break;
            case CMD_PLAY:
                next_track = next.track;
                next_repeats = next.repeats;
                break;
            case CMD_TEST:
                if (!test) {
                    ESP_LOGI(TAG, "volume test ignored: sound playing");
                    continue;
                }
                next_track = next.track;
                follow = next;
                break;
            }
            if (follow.type != CMD_TEST) {
                follow = (cmd_t){ .type = CMD_PLAY, .track = next_track, .repeats = next_repeats };
            }
            if (test) {
                /* Any new sound ends the test without the interruption rules. */
                if (next_track == 0) continue;
                ESP_LOGI(TAG, "track %d interrupts volume test", next_track);
                dfplayer_stop();
                s_next = follow;
                s_have_next = true;
                return;
            }
            if (next_track == 0) {
                /* A newer silent transition cancels a deferred sound. */
                if (next.type == CMD_TRANSITION) has_deferred = false;
                continue;
            }
            switch (notify_sound_decision(track, next_track)) {
            case SOUND_IGNORE:
                has_deferred = false;   /* state went back to what is playing */
                continue;
            case SOUND_DEFER:
                ESP_LOGI(TAG, "track %d after track %d", next_track, track);
                deferred = (cmd_t){ .type = CMD_PLAY, .track = next_track, .repeats = next_repeats };
                has_deferred = true;
                continue;
            case SOUND_INTERRUPT:
                break;
            }
            ESP_LOGI(TAG, "track %d interrupts track %d", next_track, track);
            dfplayer_stop();
            s_next = follow;
            s_have_next = true;
            return;
        }
    }
    if (has_deferred) {
        s_next = deferred;
        s_have_next = true;
    }
}

static void handle_button(button_id_t id)
{
    /* Local buttons take the same path as MQTT: through alert_state_apply. */
    int64_t now = clock_now();
    alert_msg_t m = { .ts = now, .src = ALERT_SRC_LOCAL };
    switch (id) {
    case BTN_ALERT: m.state = ALERT_STATE_RED;   break;
    case BTN_CLEAR: m.state = ALERT_STATE_CLEAR; break;
    default: return; /* stop is handled via s_stop_req */
    }
    ESP_LOGI(TAG, "local button -> %s", alert_state_name(m.state));
    if (!alert_state_apply(&m, now)) ESP_LOGI(TAG, "already %s", alert_state_name(m.state));
}

static void handle_apply(const alert_msg_t *m)
{
    alert_state_apply(m, clock_now());
}

/* Log the transition, set the LED, and return the track to play (0 = silent). */
static int transition_effects(const cmd_t *c, int *repeats)
{
    alert_log_record(c->state, c->ts, c->src);
    s_base = pattern_for(c->state);
    led_set(s_base);
    int track = notify_track_for(c->old_state, c->state, c->stale);
    if (track == 0) {
        ESP_LOGI(TAG, "%s -> %s: silent%s", alert_state_name(c->old_state),
                 alert_state_name(c->state), c->stale ? " (stale)" : "");
    }
    *repeats = c->state == ALERT_STATE_RED ? CONFIG_AA_SIREN_REPEATS : 1;
    return track;
}

/* Idle tick (nothing playing): slow blink while the broker is unreachable
 * for longer than CONFIG_AA_BROKER_LOST_SEC. Only touches the LED on change,
 * so patterns set elsewhere (setup AP, DFPlayer error) are kept. */
static void check_broker(void)
{
    int64_t since = mqtt_disconnected_since();
    int64_t up = esp_timer_get_time() / 1000000;
    bool lost = since > 0 && up - since > CONFIG_AA_BROKER_LOST_SEC;
    if (lost == s_broker_lost) return;
    s_broker_lost = lost;
    ESP_LOGW(TAG, "%s", lost ? "broker unreachable" : "broker back");
    led_set(lost ? LED_BLINK_0_5HZ : s_base);
}

static void notify_task(void *arg)
{
    (void)arg;
    const dfplayer_config_t df = {
        .uart_num = CONFIG_AA_DF_UART,
        .tx_gpio = CONFIG_AA_DF_TX_GPIO,
        .rx_gpio = CONFIG_AA_DF_RX_GPIO,
    };
    bool ok = dfplayer_init(&df) == ESP_OK && dfplayer_wait_ready(5000);
    if (!ok) {
        ESP_LOGE(TAG, "DFPlayer not ready: check SD card and wiring");
        led_set(LED_BLINK_2HZ);
    }
    apply_volume(volume_now());

    cmd_t c;
    for (;;) {
        if (s_have_next) {
            c = s_next;
            s_have_next = false;
        } else if (xQueueReceive(s_q, &c, pdMS_TO_TICKS(1000)) != pdTRUE) {
            check_broker();
            continue;
        }
        switch (c.type) {
        case CMD_PLAY:       play(c.track, c.repeats, -1); break;
        case CMD_TEST:       play(c.track, 1, c.vol); break;
        case CMD_STOP:       dfplayer_stop(); break;
        case CMD_TRANSITION: {
            int reps;
            int t = transition_effects(&c, &reps);
            if (t) play(t, reps, -1);
            break;
        }
        case CMD_BUTTON:     handle_button(c.button); break;
        case CMD_APPLY:      handle_apply(&c.msg); break;
        }
    }
}

esp_err_t notify_init(void)
{
    alert_log_init();
    s_q = xQueueCreate(12, sizeof(cmd_t));
    if (!s_q) return ESP_ERR_NO_MEM;
    alert_state_set_callback(on_state_change);
    button_init(on_button);
    if (xTaskCreate(notify_task, "notify", 4096, NULL, 5, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
