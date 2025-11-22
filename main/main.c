#include <stdio.h>
#include <string.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ssd1322.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

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

// 创建LVGL UI
static void create_demo_ui(lv_disp_t *disp)
{
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    
    // 创建标题标签
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "SSD1322 OLED");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
    
    // 创建进度条
    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 200, 10);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, -5);
    lv_bar_set_value(bar, 70, LV_ANIM_OFF);
    
    // 创建按钮
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 100, 30);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "ESP-IDF");
    lv_obj_center(btn_label);
    
    // 创建计数器标签
    lv_obj_t *counter_label = lv_label_create(scr);
    lv_label_set_text(counter_label, "Counter: 0");
    lv_obj_align(counter_label, LV_ALIGN_CENTER, 0, 15);
}

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
    ESP_LOGI(TAG, "SSD1322 Direct SPI Test");
    
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
        .flags = 0,
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
    
    // 初始化序列（与原代码完全一致）
    ESP_LOGI(TAG, "Initializing SSD1322...");
    
    send_cmd(0xFD); send_data(0x12);  // Unlock
    send_cmd(0xAE);                     // Display OFF
    send_cmd(0xB3); send_data(0x91);  // Clock divider
    send_cmd(0xCA); send_data(0x3F);  // Multiplex ratio
    send_cmd(0xA2); send_data(0x00);  // Display offset
    send_cmd(0xA1); send_data(0x00);  // Start line
    send_cmd(0xA0); send_data(0x14); send_data(0x11);  // Remap
    send_cmd(0xAB); send_data(0x01);  // Function selection
    send_cmd(0xB4); send_data(0xA0); send_data(0xFD);  // Display enhancement
    send_cmd(0xC1); send_data(0x80);  // Contrast
    send_cmd(0xC7); send_data(0x0F);  // Master contrast
    send_cmd(0xB1); send_data(0xE2);  // Phase length
    send_cmd(0xD1); send_data(0x82); send_data(0x20);  // Enhancement B
    send_cmd(0xBB); send_data(0x1F);  // Precharge voltage
    send_cmd(0xB6); send_data(0x08);  // Second precharge
    send_cmd(0xBE); send_data(0x07);  // VCOMH
    send_cmd(0xA6);                     // Normal display
    send_cmd(0xAF);                     // Display ON
    
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Initialization complete");
    
    // 测试：填充全屏白色
    ESP_LOGI(TAG, "Test: Fill white");
    send_cmd(0x15); send_data(0x1C); send_data(0x5B);  // Column address
    send_cmd(0x75); send_data(0x00); send_data(0x3F);  // Row address
    send_cmd(0x5C);  // Write RAM
    
    gpio_set_level(PIN_NUM_DC, 1);
    uint8_t white_data[128];
    memset(white_data, 0xFF, sizeof(white_data));
    
    for (int row = 0; row < 64; row++) {
        spi_transaction_t t = {
            .length = 128 * 8,
            .tx_buffer = white_data
        };
        spi_device_polling_transmit(g_spi, &t);
    }
    
    ESP_LOGI(TAG, "White screen sent");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}