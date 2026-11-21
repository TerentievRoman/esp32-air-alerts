/* Host stub: ESP-IDF logging -> printf. */
#pragma once
#include <stdio.h>

#define ESP_LOG_STUB(lvl, tag, fmt, ...) printf(lvl " (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) ESP_LOG_STUB("E", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) ESP_LOG_STUB("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) ESP_LOG_STUB("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
