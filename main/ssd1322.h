#ifndef SSD1322_H
#define SSD1322_H

#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

// GPIO引脚定义
#define OLED_SCK    GPIO_NUM_3   // SCL
#define OLED_MOSI   GPIO_NUM_18  // SDA
#define OLED_RES    GPIO_NUM_10  // RST
#define OLED_CS     GPIO_NUM_0   // CS
#define OLED_DC     GPIO_NUM_1   // DC

// SPI配置
#define SPI_HOST_ID     SPI2_HOST
#define SPI_CLOCK_HZ    (10 * 1000 * 1000)  // 10MHz

// 显示尺寸
#define OLED_WIDTH      256
#define OLED_HEIGHT     64

/**
 * @brief 初始化SSD1322 OLED显示屏
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t ssd1322_init(void);

/**
 * @brief 发送命令到SSD1322
 * @param cmd 命令字节
 */
void ssd1322_command(uint8_t cmd);

/**
 * @brief 发送数据到SSD1322
 * @param dat 数据字节
 */
void ssd1322_data(uint8_t dat);

/**
 * @brief 开始连续数据传输
 */
void ssd1322_start_data(void);

/**
 * @brief 结束连续数据传输
 */
void ssd1322_end_data(void);

/**
 * @brief 在连续传输模式下发送数据
 * @param dat 数据字节
 */
void ssd1322_send_data(uint8_t dat);

/**
 * @brief 设置显示窗口
 * @param x_start 起始X坐标（0-63）
 * @param y_start 起始Y坐标（0-63或0-127）
 * @param x_end 结束X坐标
 * @param y_end 结束Y坐标
 */
void ssd1322_set_window(uint8_t x_start, uint8_t y_start, uint8_t x_end, uint8_t y_end);

/**
 * @brief 清除屏幕（上半部分64x64）
 */
void ssd1322_clear(void);

/**
 * @brief 清除整个屏幕（256x64）
 */
void ssd1322_clear_all(void);

/**
 * @brief 显示单个字符
 * @param x X坐标（像素）
 * @param y Y坐标（像素）
 * @param ch 要显示的字符
 * @param mode 0-正常，1-反色
 */
void ssd1322_char(uint8_t x, uint8_t y, char ch, uint8_t mode);

/**
 * @brief 显示字符串
 * @param x X坐标（像素）
 * @param y Y坐标（像素）
 * @param str 字符串
 * @param mode 0-正常，1-反色
 */
void ssd1322_string(uint8_t x, uint8_t y, const char *str, uint8_t mode);

/**
 * @brief 显示单色位图
 * @param bitmap 位图数据（256x64位）
 */
void ssd1322_bitmap_mono(const uint8_t *bitmap);

/**
 * @brief 显示灰度位图
 * @param bitmap 位图数据（256x64x4位灰度）
 */
void ssd1322_bitmap_gray(const uint8_t *bitmap);

#endif // SSD1322_H
