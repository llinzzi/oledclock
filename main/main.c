#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1322_driver.h"
#include "lvgl_adapter.h"
#include "ui.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting SSD1322 OLED with LVGL");
    
    // 初始化SSD1322驱动
    ESP_ERROR_CHECK(ssd1322_init());
    
    // 初始化LVGL适配层
    ESP_ERROR_CHECK(lvgl_adapter_init());
    
    // 创建UI界面
    ESP_ERROR_CHECK(ui_init());
    
    ESP_LOGI(TAG, "All initialized successfully");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}