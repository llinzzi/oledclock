#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
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

// LVGL flush callback - L8格式转I4
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int x_start = area->x1;
    int y_start = area->y1;
    int x_end = area->x2;
    int y_end = area->y2;
    
    ESP_LOGI(TAG, "LVGL flush: x=%d-%d, y=%d-%d", x_start, x_end, y_start, y_end);
    
    int width = x_end - x_start + 1;
    int height = y_end - y_start + 1;
    
    // LVGL使用L8格式（每像素1字节），需要转换为I4（每字节2像素）
    size_t i4_len = (width / 2) * height;
    uint8_t *i4_data = heap_caps_malloc(i4_len, MALLOC_CAP_DMA);
    
    if (i4_data) {
        // 转换L8到I4：每2个L8像素合并为1个I4字节
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x += 2) {
                int src_idx = y * width + x;
                int dst_idx = y * (width / 2) + (x / 2);
                
                uint8_t p0 = px_map[src_idx] >> 4;      // 第1个像素，取高4位
                uint8_t p1 = px_map[src_idx + 1] >> 4;  // 第2个像素，取高4位
                
                i4_data[dst_idx] = (p0 << 4) | p1;  // 合并为1字节
            }
        }
        
        ESP_LOGI(TAG, "Converted L8->I4: first bytes 0x%02X 0x%02X (from 0x%02X 0x%02X)", 
                 i4_data[0], i4_data[1], px_map[0], px_map[1]);
        
        // 设置显示区域
        send_cmd(0x15);
        send_data((x_start / 4) + 0x1C);
        send_data((x_end / 4) + 0x1C);
        
        send_cmd(0x75);
        send_data(y_start);
        send_data(y_end);
        
        send_cmd(0x5C); // Write RAM
        
        gpio_set_level(PIN_NUM_DC, 1);
        
        spi_transaction_t t = {
            .length = i4_len * 8,
            .tx_buffer = i4_data
        };
        spi_device_polling_transmit(g_spi, &t);
        
        free(i4_data);
    }
    
    lv_display_flush_ready(disp);
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
    ESP_LOGI(TAG, "SSD1322 Init complete");
    
    // 不填充白屏，直接让LVGL渲染
    
    // 初始化LVGL
    ESP_LOGI(TAG, "Initializing LVGL...");
    
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));
    
    // 手动创建LVGL显示器（不使用panel_handle）
    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    
    // 设置颜色格式为L8（8位灰度，每像素1字节）
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8);
    
    // 分配缓冲区（L8格式：每像素1字节）
    size_t buf_size = LCD_H_RES * LCD_V_RES;  // 8-bit, 1 pixel per byte
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    lv_display_set_buffers(disp, buf1, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // 设置flush回调
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    
    // 创建LVGL UI - 灰度模式：0x0=黑, 0xF=白
    lv_obj_t *scr = lv_scr_act();
    
    // 设置屏幕背景为白色 (灰度0xF)
    lv_obj_set_style_bg_color(scr, lv_color_make(0xFF, 0xFF, 0xFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "SSD1322 OLED");
    // 设置文字颜色为黑色 (灰度0x0)
    lv_obj_set_style_text_color(label, lv_color_make(0x00, 0x00, 0x00), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
    
    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 200, 10);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
    // 设置进度条颜色
    lv_obj_set_style_bg_color(bar, lv_color_make(0xCC, 0xCC, 0xCC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_make(0x00, 0x00, 0x00), LV_PART_INDICATOR);
    lv_bar_set_value(bar, 75, LV_ANIM_OFF);
    
    ESP_LOGI(TAG, "LVGL UI created");
    
    // 强制LVGL刷新显示
    lv_obj_invalidate(scr);
    lv_refr_now(disp);
    
    ESP_LOGI(TAG, "Display refreshed");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}