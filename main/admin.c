#include "admin.h"

#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_netif.h"
#include "dns_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "admin_form.h"
#include "admin_page.h"
#include "alert_log.h"
#include "alert_state.h"
#include "clock.h"
#include "mqtt.h"
#include "notify.h"
#include "volume.h"
#include "wifi.h"

static const char *TAG = "admin";

#define BODY_MAX 2048
#define AP_URL   "http://192.168.4.1/"

static settings_t *s_settings;
static bool s_captive;
static esp_timer_handle_t s_restart_timer;

static void do_restart(void *arg)
{
    (void)arg;
    esp_restart();
}

static void restart_soon(void)
{
    if (!s_restart_timer) {
        const esp_timer_create_args_t a = { .callback = do_restart, .name = "restart" };
        esp_timer_create(&a, &s_restart_timer);
    }
    esp_timer_start_once(s_restart_timer, 1000 * 1000);
}

/* Returns true if the request may proceed; otherwise a 401 has been sent. */
static bool authorized(httpd_req_t *req)
{
    if (s_captive) return true;

    char hdr[200] = "";
    size_t n = httpd_req_get_hdr_value_len(req, "Authorization");
    if (n > 0 && n < sizeof(hdr)) httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr));
    if (admin_basic_auth_ok(hdr, "admin", s_settings->admin_pass)) return true;

    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"air-alert\", charset=\"UTF-8\"");
    httpd_resp_sendstr(req, "Unauthorized");
    return false;
}

static esp_err_t send_json(httpd_req_t *req, cJSON *obj)
{
    char *txt = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (!txt) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, txt);
    free(txt);
    return err;
}

static esp_err_t h_root(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, ADMIN_HTML, sizeof(ADMIN_HTML) - 1);
}

/* Non-secret settings to prefill the form. */
static esp_err_t h_config(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "ssid", s_settings->wifi_ssid);
    cJSON_AddStringToObject(o, "muri", s_settings->mqtt_uri);
    cJSON_AddStringToObject(o, "muser", s_settings->mqtt_user);
    cJSON_AddStringToObject(o, "mtopic", s_settings->mqtt_topic);
    return send_json(req, o);
}

static esp_err_t h_status(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    char id[7];
    wifi_get_device_id(id);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "state", alert_state_name(alert_state_get()));
    cJSON_AddBoolToObject(o, "wifi", wifi_is_connected());
    cJSON_AddBoolToObject(o, "mqtt", mqtt_is_connected());
    cJSON_AddBoolToObject(o, "time_synced", clock_is_synced());
    cJSON_AddStringToObject(o, "id", id);
    cJSON_AddNumberToObject(o, "volume", volume_now());
    return send_json(req, o);
}

/* Copy a form field into dst. Returns false (and names the field in *bad)
 * if the decoded value does not fit. */
static bool take(const char *body, const char *key, char *dst, size_t len, bool keep_if_empty,
                 const char **bad)
{
    char tmp[129];
    int r = admin_form_field(body, key, tmp, len < sizeof(tmp) ? len : sizeof(tmp));
    if (r < 0) {
        *bad = key;
        return false;
    }
    if (r == 0 || (keep_if_empty && tmp[0] == '\0')) return true;
    strlcpy(dst, tmp, len);
    return true;
}

static esp_err_t h_save(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    if (req->content_len == 0 || req->content_len >= BODY_MAX) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    }
    char *body = malloc(BODY_MAX);
    if (!body) return httpd_resp_send_500(req);
    int got = 0;
    while (got < (int)req->content_len) {
        int r = httpd_req_recv(req, body + got, req->content_len - got);
        if (r <= 0) { free(body); return ESP_FAIL; }
        got += r;
    }
    body[got] = '\0';

    settings_t n = *s_settings;
    const char *bad = NULL;
    bool ok = take(body, "ssid",   n.wifi_ssid,  sizeof(n.wifi_ssid),  false, &bad)
           && take(body, "wpass",  n.wifi_pass,  sizeof(n.wifi_pass),  true,  &bad)
           && take(body, "muri",   n.mqtt_uri,   sizeof(n.mqtt_uri),   false, &bad)
           && take(body, "muser",  n.mqtt_user,  sizeof(n.mqtt_user),  false, &bad)
           && take(body, "mpass",  n.mqtt_pass,  sizeof(n.mqtt_pass),  true,  &bad)
           && take(body, "mtopic", n.mqtt_topic, sizeof(n.mqtt_topic), false, &bad)
           && take(body, "apass",  n.admin_pass, sizeof(n.admin_pass), true,  &bad);
    free(body);
    if (!ok) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Field '%s' is too long, nothing saved", bad);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, msg);
    }

    if (settings_save(&n) != ESP_OK) return httpd_resp_send_500(req);
    *s_settings = n;

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_sendstr(req, "<!doctype html><meta charset=\"utf-8\"><meta name=\"viewport\" "
                            "content=\"width=device-width\"><p>Збережено, перезавантаження…</p>");
    restart_soon();
    return ESP_OK;
}

static esp_err_t h_test(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    char q[32], v[8];
    int track = 0;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK
        && httpd_query_key_value(q, "track", v, sizeof(v)) == ESP_OK) {
        track = atoi(v);
    }
    if (track < 1 || track > 3) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "track 1..3");
    notify_play(track);
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t h_volume_get(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    volume_sched_t s;
    volume_get(&s);
    char *txt = volume_sched_to_json(&s);
    if (!txt) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, txt);
    free(txt);
    return err;
}

static esp_err_t h_volume_set(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    if (req->content_len == 0 || req->content_len >= VOLUME_JSON_MAX) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
    }
    char *body = malloc(VOLUME_JSON_MAX);
    if (!body) return httpd_resp_send_500(req);
    int got = 0;
    while (got < (int)req->content_len) {
        int r = httpd_req_recv(req, body + got, req->content_len - got);
        if (r <= 0) { free(body); return ESP_FAIL; }
        got += r;
    }
    body[got] = '\0';

    volume_sched_t s;
    char err[128];
    bool ok = volume_sched_from_json(body, &s, err, sizeof(err));
    free(body);
    if (!ok) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_sendstr(req, err);
    }
    if (volume_set(&s) != ESP_OK) return httpd_resp_send_500(req);
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t h_volume_test(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    char q[32], v[8];
    int track = 0, vol = -1;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        if (httpd_query_key_value(q, "track", v, sizeof(v)) == ESP_OK) track = atoi(v);
        if (httpd_query_key_value(q, "vol", v, sizeof(v)) == ESP_OK) vol = atoi(v);
    }
    if (track < 1 || track > 3 || vol < 0 || vol > VOLUME_MAX) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "track 1..3, vol 0..30");
    }
    if (!notify_test_volume(track, vol)) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_set_type(req, "text/plain; charset=utf-8");
        return httpd_resp_sendstr(req, "Зараз грає звук");
    }
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t h_reset(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    settings_erase();
    httpd_resp_sendstr(req, "ok");
    restart_soon();
    return ESP_OK;
}

#define LOG_JSON_MAX 8192

static esp_err_t h_log(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    char *buf = malloc(LOG_JSON_MAX);
    if (!buf) return httpd_resp_send_500(req);
    int n = alert_log_to_json(buf, LOG_JSON_MAX);
    if (n < 0) {
        free(buf);
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, buf, n);
    free(buf);
    return err;
}

static esp_err_t h_log_clear(httpd_req_t *req)
{
    if (!authorized(req)) return ESP_OK;
    alert_log_clear();
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t h_redirect(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", AP_URL);
    /* iOS needs a body to detect the captive portal. */
    return httpd_resp_sendstr(req, "Redirect");
}

static esp_err_t h_404_redirect(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    return h_redirect(req);
}

esp_err_t admin_start(settings_t *s, bool captive)
{
    s_settings = s;
    s_captive = captive;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 16;
    cfg.lru_purge_enable = true;
    httpd_handle_t srv = NULL;
    esp_err_t err = httpd_start(&srv, &cfg);
    if (err != ESP_OK) return err;

    const httpd_uri_t routes[] = {
        { .uri = "/",       .method = HTTP_GET,  .handler = h_root },
        { .uri = "/config", .method = HTTP_GET,  .handler = h_config },
        { .uri = "/status", .method = HTTP_GET,  .handler = h_status },
        { .uri = "/save",   .method = HTTP_POST, .handler = h_save },
        { .uri = "/test",   .method = HTTP_POST, .handler = h_test },
        { .uri = "/reset",  .method = HTTP_POST, .handler = h_reset },
        { .uri = "/log",    .method = HTTP_GET,  .handler = h_log },
        { .uri = "/log/clear", .method = HTTP_POST, .handler = h_log_clear },
        { .uri = "/volume", .method = HTTP_GET,  .handler = h_volume_get },
        { .uri = "/volume", .method = HTTP_POST, .handler = h_volume_set },
        { .uri = "/volume/test", .method = HTTP_POST, .handler = h_volume_test },
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(srv, &routes[i]);
    }

    if (captive) {
        static const char *probes[] = { "/generate_204", "/hotspot-detect.html", "/connecttest.txt" };
        for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
            const httpd_uri_t u = { .uri = probes[i], .method = HTTP_GET, .handler = h_redirect };
            httpd_register_uri_handler(srv, &u);
        }
        httpd_register_err_handler(srv, HTTPD_404_NOT_FOUND, h_404_redirect);

        static dns_server_config_t dns = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
        start_dns_server(&dns);
    }
    ESP_LOGI(TAG, "started (%s)", captive ? "captive, no auth" : "basic auth");
    return ESP_OK;
}
