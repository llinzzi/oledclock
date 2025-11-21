#include <stdio.h>
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

// GPIO配置
#define LCD_HOST       SPI2_HOST
#define PIN_NUM_MOSI   18
#define PIN_NUM_CLK    3
#define PIN_NUM_CS     0
#define PIN_NUM_DC     1
#define PIN_NUM_RST    10

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

void app_main(void)
{
    ESP_LOGI(TAG, "SSD1322 OLED + LVGL Demo (esp_lvgl_port)");
    
    // 配置SPI总线
    spi_bus_config_t buscfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES / 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    // 配置LCD IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    
    // 配置LCD面板
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1322(io_handle, &panel_config, &panel_handle));
    
    // 复位并初始化
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    
    ESP_LOGI(TAG, "SSD1322 initialized successfully");
    
    // 测试1: 全屏填充白色
    ESP_LOGI(TAG, "Test 1: Fill white");
    size_t test_buf_size = (LCD_H_RES / 2) * LCD_V_RES;
    uint8_t *test_buffer = heap_caps_malloc(test_buf_size, MALLOC_CAP_DMA);
    if (test_buffer) {
        memset(test_buffer, 0xFF, test_buf_size); // 全白 (0xF = 最亮)
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES - 1, LCD_V_RES - 1, test_buffer));
        ESP_LOGI(TAG, "White screen displayed");
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // 测试2: 全屏填充黑色
        ESP_LOGI(TAG, "Test 2: Fill black");
        memset(test_buffer, 0x00, test_buf_size); // 全黑
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES - 1, LCD_V_RES - 1, test_buffer));
        ESP_LOGI(TAG, "Black screen displayed");
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // 测试3: 灰度渐变
        ESP_LOGI(TAG, "Test 3: Grayscale gradient");
        for (int y = 0; y < LCD_V_RES; y++) {
            for (int x = 0; x < LCD_H_RES / 2; x++) {
                uint8_t gray1 = (x * 2 * 15) / (LCD_H_RES - 1);
                uint8_t gray2 = ((x * 2 + 1) * 15) / (LCD_H_RES - 1);
                test_buffer[y * (LCD_H_RES / 2) + x] = (gray1 << 4) | gray2;
            }
        }
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES - 1, LCD_V_RES - 1, test_buffer));
        ESP_LOGI(TAG, "Gradient displayed");
        
        free(test_buffer);
    }
    
    ESP_LOGI(TAG, "Test complete, entering main loop");
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}