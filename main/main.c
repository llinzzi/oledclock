#include <stdio.h>
#include "ssd1322.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "SSD1322 OLED Test Starting...");
    
    // 初始化SSD1322
    esp_err_t ret = ssd1322_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SSD1322: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "SSD1322 initialized successfully");
    
    // 清除屏幕
    ssd1322_clear_all();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 显示测试文本
    ssd1322_string(0, 0, "Hello SSD1322!", 0);
    ssd1322_string(0, 16, "ESP-IDF 5.5", 0);
    ssd1322_string(0, 32, "256x64 OLED", 0);
    ssd1322_string(0, 48, "Test Display", 0);
    
    ESP_LOGI(TAG, "Test display complete");
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}