#include "mqtt.h"

#include <stdio.h>
#include <string.h>
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"

#include "alert_state.h"
#include "clock.h"
#include "wifi.h"

static const char *TAG = "mqtt";

static esp_mqtt_client_handle_t s_client;
static char s_topic[96];
static char s_status_topic[64];
static volatile bool s_connected;
static volatile int64_t s_lost_since;

/* Uptime in seconds, never 0 (0 means "connected" for mqtt_disconnected_since). */
static int64_t uptime_s(void) { return esp_timer_get_time() / 1000000 + 1; }

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
    alert_state_apply(&m, clock_now());
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
    s_lost_since = uptime_s();

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
