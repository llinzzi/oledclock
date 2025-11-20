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
    
    // 灰度测试
    ESP_LOGI(TAG, "Starting grayscale test...");
    
    // 绘制灰度渐变条
    ssd1322_set_window(0, 0, 63, 63);
    ssd1322_start_data();
    
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 128; x += 2) {
            // 每个像素用4位表示灰度（0-15）
            // 横向渐变：从左到右，从暗到亮
            uint8_t gray1 = (x * 15) / 127;
            uint8_t gray2 = ((x + 1) * 15) / 127;
            uint8_t pixel_data = (gray1 << 4) | gray2;
            ssd1322_send_data(pixel_data);
        }
    }
    
    ssd1322_end_data();
    
    ESP_LOGI(TAG, "Grayscale gradient test complete");
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 显示文本
    ssd1322_clear_all();
    ssd1322_string(0, 0, "Grayscale Test", 0);
    ssd1322_string(0, 20, "SSD1322 OLED", 0);
    ssd1322_string(0, 40, "ESP-IDF 5.5", 0);
    
    ESP_LOGI(TAG, "Test complete");
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}