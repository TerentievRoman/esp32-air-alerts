#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/uart.h"

/*
 * Мінімальний драйвер DFPlayer Mini (UART 9600 8N1).
 *
 * Важливо: DFPlayer не вміє відкривати файл за ім'ям. Команда play_track(N)
 * грає N-й файл у порядку запису у FAT (порядок копіювання на картку),
 * а не за алфавітом.
 */

typedef struct {
    uart_port_t uart_num;
    int tx_gpio;   /* ESP TX -> RX DFPlayer (бажано через резистор 1 кОм) */
    int rx_gpio;   /* ESP RX <- TX DFPlayer */
} dfplayer_config_t;

esp_err_t dfplayer_init(const dfplayer_config_t *cfg);

/* Чекає повідомлення DFPlayer про готовність картки (0x3F). */
bool dfplayer_wait_ready(uint32_t timeout_ms);

esp_err_t dfplayer_set_volume(uint8_t vol); /* 0..30 */
esp_err_t dfplayer_play_track(uint16_t n);  /* 1..2999, порядок у FAT */
esp_err_t dfplayer_stop(void);

/* Чекає завершення треку (0x3D). false — таймаут або помилка плеєра. */
bool dfplayer_wait_finished(uint32_t timeout_ms);
