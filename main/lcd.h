#pragma once
/* PCD8544 (Nokia 5110) 84x48 LCD on SPI2; pins from CONFIG_AA_LCD_*. */
#include <stdbool.h>
#include "esp_err.h"

#define LCD_WIDTH  84
#define LCD_HEIGHT 48

esp_err_t lcd_init(void);

/* Draw into the frame buffer; call lcd_flush() to show it. */
void lcd_clear(void);

/* Text at character cell (col 0..13, row 0..5), 6x8 cells, UTF-8.
 * invert: the whole row becomes white text on black. */
void lcd_text(int col, int row, const char *utf8, bool invert);

esp_err_t lcd_flush(void);
