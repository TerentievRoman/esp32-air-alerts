#include "mqtt.h"

#include <stdio.h>
#include <string.h>
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"

#include "alert_state.h"
#include "clock.h"
#include "notify.h"
#include "wifi.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_client;
static char s_topic[96];
static char s_status_topic[64];
static volatile bool s_connected;
static volatile int64_t s_lost_since;
static int64_t s_last_ts;              /* ts of the last accepted state message */

/* Retained message received before SNTP sync: held until sync or timeout. */
#define PENDING_MAX_WAIT_S 10
static alert_msg_t s_pending;
static bool s_has_pending;
static esp_timer_handle_t s_pending_timer;
static portMUX_TYPE s_pending_mux = portMUX_INITIALIZER_UNLOCKED;

/* Uptime in seconds, never 0 (0 means "connected" for mqtt_disconnected_since). */
static int64_t uptime_s(void) { return esp_timer_get_time() / 1000000 + 1; }

static void flush_pending(const char *why)
{
    alert_msg_t m;
    taskENTER_CRITICAL(&s_pending_mux);
    bool have = s_has_pending;
    m = s_pending;
    s_has_pending = false;
    taskEXIT_CRITICAL(&s_pending_mux);
    if (!have) return;
    ESP_LOGI(TAG, "applying held retained %s (%s)", alert_state_name(m.state), why);
    notify_apply(&m);
}

static void pending_timeout(void *arg)
{
    (void)arg;
    flush_pending("time still unknown: fail-loud");
}

void mqtt_on_time_sync(void)
{
    if (s_pending_timer) esp_timer_stop(s_pending_timer);
    flush_pending("time synced");
}

void mqtt_expect(void)
{
    if (s_lost_since == 0) s_lost_since = uptime_s();
}

static void handle_data(const esp_mqtt_event_t *e)
{
    if (e->current_data_offset != 0 || e->data_len != e->total_data_len) {
        ESP_LOGW(TAG, "fragmented payload ignored (%d bytes)", e->total_data_len);
        return;
    }
    if (e->topic_len != (int)strlen(s_topic) || strncmp(e->topic, s_topic, e->topic_len) != 0) {
        return;
    }
    alert_msg_t m;
    if (!alert_state_parse(e->data, e->data_len, &m)) {
        ESP_LOGW(TAG, "bad payload: %.*s", e->data_len > 80 ? 80 : e->data_len, e->data);
        return;
    }
    ESP_LOGI(TAG, "state %s ts %lld%s", alert_state_name(m.state), (long long)m.ts,
             e->retain ? " (retained)" : "");
    if (alert_msg_is_replay(e->retain, m.ts, s_last_ts)) {
        ESP_LOGI(TAG, "retained re-delivery, ignored");
        return;
    }
    if (m.ts > s_last_ts) s_last_ts = m.ts;

    if (e->retain && !clock_is_synced()) {
        /* Can't tell yet whether it is stale: wait for SNTP (bounded). */
        taskENTER_CRITICAL(&s_pending_mux);
        s_pending = m;
        s_has_pending = true;
        taskEXIT_CRITICAL(&s_pending_mux);
        if (!s_pending_timer) {
            const esp_timer_create_args_t a = { .callback = pending_timeout, .name = "mqtt_pend" };
            esp_timer_create(&a, &s_pending_timer);
        }
        esp_timer_stop(s_pending_timer);
        esp_timer_start_once(s_pending_timer, PENDING_MAX_WAIT_S * 1000000ULL);
        ESP_LOGI(TAG, "time not synced yet: holding retained state up to %d s", PENDING_MAX_WAIT_S);
        return;
    }
    notify_apply(&m);
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    esp_mqtt_event_handle_t e = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        s_connected = true;
        s_lost_since = 0;
        esp_mqtt_client_publish(s_client, s_status_topic, "online", 0, 1, 1);
        esp_mqtt_client_subscribe(s_client, s_topic, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        if (s_connected) ESP_LOGW(TAG, "MQTT disconnected");
        s_connected = false;
        if (s_lost_since == 0) s_lost_since = uptime_s();
        break;
    case MQTT_EVENT_DATA:
        handle_data(e);
        break;
    case MQTT_EVENT_ERROR:
        if (e->error_handle && e->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(TAG, "connection refused (code %d): check login/password",
                     e->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "error");
        }
        break;
    default:
        break;
    }
}

esp_err_t mqtt_start(const settings_t *s)
{
    char id[7];
    wifi_get_device_id(id);
    strlcpy(s_topic, s->mqtt_topic, sizeof(s_topic));
    snprintf(s_status_topic, sizeof(s_status_topic), "air_alert/kyiv/device/%s/status", id);
    mqtt_expect();

    char client_id[24];
    snprintf(client_id, sizeof(client_id), "air-alert-%s", id);

    const esp_mqtt_client_config_t cfg = {
        .broker.address.uri = s->mqtt_uri,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.client_id = client_id,
        .credentials.username = s->mqtt_user[0] ? s->mqtt_user : NULL,
        .credentials.authentication.password = s->mqtt_pass[0] ? s->mqtt_pass : NULL,
        .session.last_will = {
            .topic = s_status_topic,
            .msg = "offline",
            .qos = 1,
            .retain = 1,
        },
        .session.keepalive = 30,
    };
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) return ESP_FAIL;
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, on_event, NULL);
    ESP_LOGI(TAG, "connecting to %s, topic %s", s->mqtt_uri, s_topic);
    return esp_mqtt_client_start(s_client);
}

bool mqtt_is_connected(void) { return s_connected; }

int64_t mqtt_disconnected_since(void) { return s_connected ? 0 : s_lost_since; }
