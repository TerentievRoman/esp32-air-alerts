#include "clock.h"

#include <stdlib.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"

static const char *TAG = "clock";

#define KYIV_TZ "EET-2EEST,M3.5.0/3,M10.5.0/4"

static volatile bool s_synced;
static void (*s_on_sync)(void);

static void sync_cb(struct timeval *tv)
{
    (void)tv;
    s_synced = true;
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    ESP_LOGI(TAG, "time synced: %s", buf);
    if (s_on_sync) s_on_sync();
}

void clock_init(void (*on_sync)(void))
{
    s_on_sync = on_sync;
    setenv("TZ", KYIV_TZ, 1);
    tzset();

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    cfg.sync_cb = sync_cb;
    esp_err_t err = esp_netif_sntp_init(&cfg);
    if (err != ESP_OK) ESP_LOGE(TAG, "sntp init: %s", esp_err_to_name(err));
}

bool clock_is_synced(void) { return s_synced; }

int64_t clock_now(void) { return s_synced ? (int64_t)time(NULL) : 0; }
