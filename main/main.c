#include <stdio.h>
#include <string.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ssd1322.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

// GPIO配置 - 按照README接线
#define LCD_HOST       SPI2_HOST
#define PIN_NUM_MOSI   12  // IO18: SDA
#define PIN_NUM_CLK    3   // IO3: SCL  
#define PIN_NUM_CS     0   // IO0: CS
#define PIN_NUM_DC     1   // IO1: DC
#define PIN_NUM_RST    10  // IO10: RST

#define LCD_H_RES      256
#define LCD_V_RES      64
#define LCD_PIXEL_CLOCK_HZ (10 * 1000 * 1000)

static spi_device_handle_t g_spi;

static void send_cmd(uint8_t c) {
    gpio_set_level(PIN_NUM_DC, 0);
    spi_transaction_t t = {.length = 8, .tx_buffer = &c};
    spi_device_polling_transmit(g_spi, &t);
}

static void send_data(uint8_t d) {
    gpio_set_level(PIN_NUM_DC, 1);
    spi_transaction_t t = {.length = 8, .tx_buffer = &d};
    spi_device_polling_transmit(g_spi, &t);
}

void app_main(void)
{
    ESP_LOGI(TAG, "SSD1322 - Pure Direct SPI (Working Version)");
    
    // 配置GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // 配置SPI总线
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    // 添加SPI设备
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &g_spi));
    
    // 硬件复位
    ESP_LOGI(TAG, "Hardware reset");
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 初始化序列
    ESP_LOGI(TAG, "Initializing SSD1322...");
    
    send_cmd(0xFD); send_data(0x12);
    send_cmd(0xAE);
    send_cmd(0xB3); send_data(0x91);
    send_cmd(0xCA); send_data(0x3F);
    send_cmd(0xA2); send_data(0x00);
    send_cmd(0xA1); send_data(0x00);
    send_cmd(0xA0); send_data(0x14); send_data(0x11);
    send_cmd(0xAB); send_data(0x01);
    send_cmd(0xB4); send_data(0xA0); send_data(0xFD);
    send_cmd(0xC1); send_data(0x80);
    send_cmd(0xC7); send_data(0x0F);
    send_cmd(0xB1); send_data(0xE2);
    send_cmd(0xD1); send_data(0x82); send_data(0x20);
    send_cmd(0xBB); send_data(0x1F);
    send_cmd(0xB6); send_data(0x08);
    send_cmd(0xBE); send_data(0x07);
    send_cmd(0xA6);
    send_cmd(0xAF);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Init complete");
    
    // 填充白色
    ESP_LOGI(TAG, "Drawing white screen");
    send_cmd(0x15); send_data(0x1C); send_data(0x5B);
    send_cmd(0x75); send_data(0x00); send_data(0x3F);
    send_cmd(0x5C);
    
    gpio_set_level(PIN_NUM_DC, 1);
    uint8_t white[128];
    memset(white, 0xFF, sizeof(white));
    
    for (int row = 0; row < 64; row++) {
        spi_transaction_t t = {.length = 128 * 8, .tx_buffer = white};
        spi_device_polling_transmit(g_spi, &t);
    }
    
    ESP_LOGI(TAG, "White screen displayed - working!");
    ESP_LOGI(TAG, "Now the problem is: why esp_lcd_panel_io doesn't work");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}