#include "lcd.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "font5x8.h"

static const char *TAG = "lcd";

#define BANKS     (LCD_HEIGHT / 8)
#define CELL_W    6
#define SPI_HZ    (4 * 1000 * 1000)

static spi_device_handle_t s_dev;
static uint8_t s_fb[BANKS][LCD_WIDTH];   /* bit 0 = top pixel of the bank */

static esp_err_t send(bool data, const uint8_t *buf, size_t len)
{
    gpio_set_level(CONFIG_AA_LCD_DC_GPIO, data ? 1 : 0);
    spi_transaction_t t = { .length = len * 8, .tx_buffer = buf };
    return spi_device_polling_transmit(s_dev, &t);
}

static esp_err_t cmd(uint8_t c) { return send(false, &c, 1); }

void lcd_preinit(void)
{
#if CONFIG_AA_LCD_ENABLE
    gpio_set_level(CONFIG_AA_LCD_RST_GPIO, 0);
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_AA_LCD_RST_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    gpio_set_level(CONFIG_AA_LCD_RST_GPIO, 0);
#endif
}

esp_err_t lcd_init(void)
{
    const gpio_config_t io = {
        .pin_bit_mask = (1ULL << CONFIG_AA_LCD_DC_GPIO) | (1ULL << CONFIG_AA_LCD_RST_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) return err;

    const spi_bus_config_t bus = {
        .mosi_io_num = CONFIG_AA_LCD_DIN_GPIO,
        .sclk_io_num = CONFIG_AA_LCD_CLK_GPIO,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(s_fb),
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi bus: %s", esp_err_to_name(err));
        return err;
    }
    const spi_device_interface_config_t dev = {
        .clock_speed_hz = SPI_HZ,
        .mode = 0,
        .spics_io_num = CONFIG_AA_LCD_CE_GPIO,
        .queue_size = 1,
    };
    err = spi_bus_add_device(SPI2_HOST, &dev, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi device: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_level(CONFIG_AA_LCD_RST_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(CONFIG_AA_LCD_RST_GPIO, 1);

    const uint8_t init[] = {
        0x21,                                  /* extended instruction set */
        0x80 | (CONFIG_AA_LCD_CONTRAST & 0x7F),/* Vop (contrast) */
        0x04,                                  /* temperature coefficient */
        0x14,                                  /* bias 1:48 */
        0x20,                                  /* basic instruction set */
        0x0C,                                  /* normal display */
    };
    for (size_t i = 0; i < sizeof(init) && err == ESP_OK; i++) err = cmd(init[i]);
    if (err != ESP_OK) return err;

    lcd_clear();
    err = lcd_flush();
    ESP_LOGI(TAG, "PCD8544 ready (contrast 0x%02X)", CONFIG_AA_LCD_CONTRAST);
    return err;
}

void lcd_clear(void) { memset(s_fb, 0, sizeof(s_fb)); }

void lcd_text(int col, int row, const char *utf8, bool invert)
{
    if (row < 0 || row >= BANKS) return;
    uint8_t *line = s_fb[row];
    if (invert) memset(line, 0xFF, LCD_WIDTH);

    int x = col * CELL_W;
    uint32_t cp;
    while ((cp = utf8_next(&utf8)) && x < LCD_WIDTH) {
        const uint8_t *g = font5x8_glyph(cp);
        for (int i = 0; i < CELL_W && x < LCD_WIDTH; i++, x++) {
            uint8_t bits = i < 5 ? g[i] : 0;
            line[x] = invert ? (uint8_t)~bits : bits;
        }
    }
}

esp_err_t lcd_flush(void)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    esp_err_t err = cmd(0x40);           /* Y = 0 */
    if (err == ESP_OK) err = cmd(0x80);  /* X = 0 */
    if (err == ESP_OK) err = send(true, &s_fb[0][0], sizeof(s_fb));
    return err;
}
