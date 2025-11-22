#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_lcd_ssd1322.h"

#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

static const char *TAG = "ssd1322";

// SSD1322 commands
#define SSD1322_CMD_SET_COMMAND_LOCK        0xFD
#define SSD1322_CMD_DISPLAY_OFF             0xAE
#define SSD1322_CMD_DISPLAY_ON              0xAF
#define SSD1322_CMD_SET_CLOCK_DIVIDER       0xB3
#define SSD1322_CMD_SET_MULTIPLEX_RATIO     0xCA
#define SSD1322_CMD_SET_DISPLAY_OFFSET      0xA2
#define SSD1322_CMD_SET_START_LINE          0xA1
#define SSD1322_CMD_SET_REMAP               0xA0
#define SSD1322_CMD_FUNCTION_SELECTION      0xAB
#define SSD1322_CMD_DISPLAY_ENHANCEMENT     0xB4
#define SSD1322_CMD_SET_CONTRAST            0xC1
#define SSD1322_CMD_MASTER_CONTRAST         0xC7
#define SSD1322_CMD_SET_PHASE_LENGTH        0xB1
#define SSD1322_CMD_DISPLAY_ENHANCEMENT_B   0xD1
#define SSD1322_CMD_SET_PRECHARGE_VOLTAGE   0xBB
#define SSD1322_CMD_SET_SECOND_PRECHARGE    0xB6
#define SSD1322_CMD_SET_VCOMH               0xBE
#define SSD1322_CMD_NORMAL_DISPLAY          0xA6
#define SSD1322_CMD_SET_COLUMN_ADDR         0x15
#define SSD1322_CMD_SET_ROW_ADDR            0x75
#define SSD1322_CMD_WRITE_RAM               0x5C

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t width;
    uint8_t height;
    uint8_t colmod;
    bool swap_axes;
    bool mirror_x;
    bool mirror_y;
    bool invert_color;
} ssd1322_panel_t;

static esp_err_t panel_ssd1322_del(esp_lcd_panel_t *panel);
static esp_err_t panel_ssd1322_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_ssd1322_init(esp_lcd_panel_t *panel);
static esp_err_t panel_ssd1322_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_ssd1322_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_ssd1322_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_ssd1322_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_ssd1322_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_ssd1322_disp_on_off(esp_lcd_panel_t *panel, bool on_off);

esp_err_t esp_lcd_new_panel_ssd1322(const esp_lcd_panel_io_handle_t io,
                                     const esp_lcd_panel_dev_config_t *panel_dev_config,
                                     esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    ssd1322_panel_t *ssd1322 = NULL;
    
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    
    ssd1322 = calloc(1, sizeof(ssd1322_panel_t));
    ESP_GOTO_ON_FALSE(ssd1322, ESP_ERR_NO_MEM, err, TAG, "no mem for ssd1322 panel");
    
    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }
    
    ssd1322->io = io;
    ssd1322->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ssd1322->reset_level = panel_dev_config->flags.reset_active_high;
    ssd1322->width = 256;
    ssd1322->height = 64;
    ssd1322->base.del = panel_ssd1322_del;
    ssd1322->base.reset = panel_ssd1322_reset;
    ssd1322->base.init = panel_ssd1322_init;
    ssd1322->base.draw_bitmap = panel_ssd1322_draw_bitmap;
    ssd1322->base.invert_color = panel_ssd1322_invert_color;
    ssd1322->base.set_gap = panel_ssd1322_set_gap;
    ssd1322->base.mirror = panel_ssd1322_mirror;
    ssd1322->base.swap_xy = panel_ssd1322_swap_xy;
    ssd1322->base.disp_on_off = panel_ssd1322_disp_on_off;
    
    *ret_panel = &(ssd1322->base);
    ESP_LOGD(TAG, "new ssd1322 panel @%p", ssd1322);
    
    return ESP_OK;

err:
    if (ssd1322) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(ssd1322);
    }
    return ret;
}

static esp_err_t panel_ssd1322_del(esp_lcd_panel_t *panel)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    
    if (ssd1322->reset_gpio_num >= 0) {
        gpio_reset_pin(ssd1322->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del ssd1322 panel @%p", ssd1322);
    free(ssd1322);
    return ESP_OK;
}

static esp_err_t panel_ssd1322_reset(esp_lcd_panel_t *panel)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1322->io;
    
    if (ssd1322->reset_gpio_num >= 0) {
        gpio_set_level(ssd1322->reset_gpio_num, ssd1322->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ssd1322->reset_gpio_num, !ssd1322->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(ssd1322->reset_gpio_num, ssd1322->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    return ESP_OK;
}

static esp_err_t panel_ssd1322_init(esp_lcd_panel_t *panel)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1322->io;
    
    ESP_LOGI(TAG, "Starting SSD1322 initialization sequence...");
    
    // Unlock commands
    ESP_LOGI(TAG, "Unlocking commands");
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_COMMAND_LOCK, (uint8_t[]) {0x12}, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Display off
    ESP_LOGI(TAG, "Display OFF");
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_DISPLAY_OFF, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Set clock divider
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_CLOCK_DIVIDER, (uint8_t[]) {0x91}, 1);
    
    // Set multiplex ratio (duty = 1/64)
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_MULTIPLEX_RATIO, (uint8_t[]) {0x3F}, 1);
    
    // Set display offset
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_DISPLAY_OFFSET, (uint8_t[]) {0x00}, 1);
    
    // Set start line
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_START_LINE, (uint8_t[]) {0x00}, 1);
    
    // Set remap
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_REMAP, (uint8_t[]) {0x14, 0x11}, 2);
    
    // Function selection (external VDD)
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_FUNCTION_SELECTION, (uint8_t[]) {0x01}, 1);
    
    // Display enhancement A
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_DISPLAY_ENHANCEMENT, (uint8_t[]) {0xA0, 0xFD}, 2);
    
    // Set contrast current - 对比度
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_CONTRAST, (uint8_t[]) {0x80}, 1);
    
    // Master contrast current control - 增加主对比度
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_MASTER_CONTRAST, (uint8_t[]) {0x0F}, 1);
    
    // Set phase length
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_PHASE_LENGTH, (uint8_t[]) {0xE2}, 1);
    
    // Display enhancement B
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_DISPLAY_ENHANCEMENT_B, (uint8_t[]) {0x82, 0x20}, 2);
    
    // Set precharge voltage
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_PRECHARGE_VOLTAGE, (uint8_t[]) {0x1F}, 1);
    
    // Set second precharge period
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_SECOND_PRECHARGE, (uint8_t[]) {0x08}, 1);
    
    // Set VCOMH
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_VCOMH, (uint8_t[]) {0x07}, 1);
    
    // Normal display mode
    ESP_LOGI(TAG, "Setting normal display mode");
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_NORMAL_DISPLAY, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Display on
    ESP_LOGI(TAG, "Display ON");
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_DISPLAY_ON, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ESP_LOGI(TAG, "SSD1322 initialization complete");
    
    return ESP_OK;
}

static esp_err_t panel_ssd1322_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1322->io;
    
    ESP_LOGI(TAG, "draw_bitmap: x=%d-%d, y=%d-%d", x_start, x_end, y_start, y_end);
    
    // SSD1322: 256x64 pixels
    // 列地址是按照 4像素/列 计算，256像素 = 64列
    // 列范围: 0-63, 但需要加偏移 0x1C (28)
    // 数据格式: 每字节包含2个像素（4位每像素）
    
    uint8_t col_start = (x_start / 4) + 0x1C;
    uint8_t col_end = (x_end / 4) + 0x1C;
    
    ESP_LOGI(TAG, "Column: 0x%02X-0x%02X, Row: %d-%d", col_start, col_end, y_start, y_end);
    
    // 设置列地址
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_COLUMN_ADDR, (uint8_t[]) {col_start, col_end}, 2);
    
    // 设置行地址
    esp_lcd_panel_io_tx_param(io, SSD1322_CMD_SET_ROW_ADDR, (uint8_t[]) {y_start, y_end}, 2);
    
    // 写入RAM
    // 每字节包含2个像素，所以数据长度 = (宽度/2) * 高度
    int width = x_end - x_start + 1;
    int height = y_end - y_start + 1;
    size_t len = (width / 2) * height;
    
    ESP_LOGI(TAG, "Sending %d bytes (w=%d, h=%d)", len, width, height);
    
    esp_lcd_panel_io_tx_color(io, SSD1322_CMD_WRITE_RAM, color_data, len);
    
    return ESP_OK;
}

static esp_err_t panel_ssd1322_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    ssd1322->invert_color = invert_color_data;
    return ESP_OK;
}

static esp_err_t panel_ssd1322_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    ssd1322->mirror_x = mirror_x;
    ssd1322->mirror_y = mirror_y;
    return ESP_OK;
}

static esp_err_t panel_ssd1322_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    ssd1322->swap_axes = swap_axes;
    return ESP_OK;
}

static esp_err_t panel_ssd1322_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    ssd1322->x_gap = x_gap;
    ssd1322->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_ssd1322_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    ssd1322_panel_t *ssd1322 = container_of(panel, ssd1322_panel_t, base);
    esp_lcd_panel_io_handle_t io = ssd1322->io;
    
    if (on_off) {
        esp_lcd_panel_io_tx_param(io, SSD1322_CMD_DISPLAY_ON, NULL, 0);
    } else {
        esp_lcd_panel_io_tx_param(io, SSD1322_CMD_DISPLAY_OFF, NULL, 0);
    }
    
    return ESP_OK;
}
