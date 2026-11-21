#include "notify.h"

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "alert_state.h"
#include "button.h"
#include "dfplayer.h"
#include "led.h"

static const char *TAG = "notify";

#define TRACK_ALERT   1
#define TRACK_CLEAR   2
#define TRACK_MAX     3
#define POLL_MS       200
/* DFPlayer may not report track end (e.g. its TX line is not wired): cap each play. */
#define TRACK_MAX_MS  (6 * 60 * 1000)

typedef enum { CMD_PLAY, CMD_STOP, CMD_TRANSITION, CMD_BUTTON } cmd_type_t;

typedef struct {
    cmd_type_t type;
    int track;              /* CMD_PLAY */
    int repeats;            /* CMD_PLAY */
    alert_state_t state;    /* CMD_TRANSITION */
    bool stale;             /* CMD_TRANSITION */
    button_id_t button;     /* CMD_BUTTON */
} cmd_t;

static QueueHandle_t s_q;
static volatile bool s_stop_req;

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

void notify_stop(void)
{
    s_stop_req = true;
    post(&(cmd_t){ .type = CMD_STOP });
}

static void on_state_change(alert_state_t old_state, alert_state_t new_state, bool stale)
{
    (void)old_state;
    post(&(cmd_t){ .type = CMD_TRANSITION, .state = new_state, .stale = stale });
}

/* Runs in the esp_timer task: only flag / enqueue. */
static void on_button(button_id_t id)
{
    if (id == BTN_STOP) s_stop_req = true;
    post(&(cmd_t){ .type = CMD_BUTTON, .button = id });
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

/* Play a track `repeats` times. Returns early on BOOT/stop or when another
 * command is waiting (it preempts the current sound). */
static void play(int track, int repeats)
{
    s_stop_req = false;
    for (int r = 0; r < repeats; r++) {
        ESP_LOGI(TAG, "play track %d (%d/%d)", track, r + 1, repeats);
        dfplayer_play_track(track);
        for (int waited = 0; waited < TRACK_MAX_MS; waited += POLL_MS) {
            if (dfplayer_wait_finished(POLL_MS)) break;
            if (s_stop_req) {
                ESP_LOGI(TAG, "stopped by button");
                dfplayer_stop();
                return;
            }
            cmd_t next;
            if (xQueuePeek(s_q, &next, 0) == pdTRUE && next.type != CMD_BUTTON) {
                ESP_LOGI(TAG, "preempted");
                dfplayer_stop();
                return;
            }
        }
    }
}

static void handle_button(button_id_t id)
{
    /* Local buttons take the same path as MQTT: through alert_state_apply. */
    alert_msg_t m = { .ts = 0 }; /* TODO Task 6: clock_now() */
    switch (id) {
    case BTN_ALERT: m.state = ALERT_STATE_RED;   break;
    case BTN_CLEAR: m.state = ALERT_STATE_CLEAR; break;
    default: return; /* BTN_STOP handled via s_stop_req */
    }
    ESP_LOGI(TAG, "local button -> %s", alert_state_name(m.state));
    if (!alert_state_apply(&m, 0)) ESP_LOGI(TAG, "already %s", alert_state_name(m.state));
}

static void handle_transition(alert_state_t s, bool stale)
{
    led_set(pattern_for(s));
    switch (s) {
    case ALERT_STATE_RED:
        if (stale) ESP_LOGW(TAG, "RED is stale: LED only, no siren");
        else play(TRACK_ALERT, CONFIG_AA_SIREN_REPEATS);
        break;
    case ALERT_STATE_YELLOW:
        play(TRACK_ALERT, 1);
        break;
    case ALERT_STATE_CLEAR:
        play(TRACK_CLEAR, 1);
        break;
    default:
        break;
    }
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
    dfplayer_set_volume(CONFIG_AA_DF_VOLUME);

    cmd_t c;
    for (;;) {
        if (xQueueReceive(s_q, &c, pdMS_TO_TICKS(1000)) != pdTRUE) {
            /* TODO Task 7: broker-lost LED (mqtt_disconnected_since). */
            continue;
        }
        switch (c.type) {
        case CMD_PLAY:       play(c.track, c.repeats); break;
        case CMD_STOP:       dfplayer_stop(); break;
        case CMD_TRANSITION: handle_transition(c.state, c.stale); break;
        case CMD_BUTTON:     handle_button(c.button); break;
        }
    }
}

esp_err_t notify_init(void)
{
    s_q = xQueueCreate(8, sizeof(cmd_t));
    if (!s_q) return ESP_ERR_NO_MEM;
    alert_state_set_callback(on_state_change);
    button_init(on_button);
    if (xTaskCreate(notify_task, "notify", 4096, NULL, 5, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
