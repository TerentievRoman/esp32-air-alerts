/* Placeholder until the notify task (Task 8): admin test buttons only log. */
#include "notify.h"
#include "esp_log.h"

static const char *TAG = "notify";

esp_err_t notify_init(void) { return ESP_OK; }
void notify_play(int track) { ESP_LOGI(TAG, "play %d (stub)", track); }
void notify_stop(void) { ESP_LOGI(TAG, "stop (stub)"); }
