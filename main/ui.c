#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "UI";

esp_err_t ui_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    
    // 设置屏幕背景为黑色 (灰度0x0)
    lv_obj_set_style_bg_color(scr, lv_color_make(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    // 创建标签 - 白色文字
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "SSD1322 OLED");
    lv_obj_set_style_text_color(label, lv_color_make(0xFF, 0xFF, 0xFF), 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
    
    // 创建进度条 - 灰底白条
    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 200, 10);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_make(0x33, 0x33, 0x33), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_INDICATOR);
    lv_bar_set_value(bar, 75, LV_ANIM_OFF);
    
    ESP_LOGI(TAG, "UI created successfully");
    return ESP_OK;
}
