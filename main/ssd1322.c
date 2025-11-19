#include "ssd1322.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "font_9x15.h"

static const char *TAG = "SSD1322";
static spi_device_handle_t spi_handle;

// 初始化GPIO
static void ssd1322_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OLED_RES) | (1ULL << OLED_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

// 初始化SPI
static esp_err_t ssd1322_spi_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = OLED_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = OLED_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret = spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = OLED_CS,
        .queue_size = 7,
        .flags = 0,
    };

    ret = spi_bus_add_device(SPI_HOST_ID, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

// 发送命令
void ssd1322_command(uint8_t cmd)
{
    gpio_set_level(OLED_DC, 0);
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
        .flags = 0,
    };
    spi_device_polling_transmit(spi_handle, &trans);
}

// 发送数据
void ssd1322_data(uint8_t dat)
{
    gpio_set_level(OLED_DC, 1);
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &dat,
        .flags = 0,
    };
    spi_device_polling_transmit(spi_handle, &trans);
}

// 开始连续数据传输
void ssd1322_start_data(void)
{
    gpio_set_level(OLED_DC, 1);
}

// 结束连续数据传输
void ssd1322_end_data(void)
{
    // 不需要特殊操作
}

// 在连续传输模式下发送数据
void ssd1322_send_data(uint8_t dat)
{
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &dat,
        .flags = 0,
    };
    spi_device_polling_transmit(spi_handle, &trans);
}

// 硬件复位
static void ssd1322_reset(void)
{
    gpio_set_level(OLED_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(OLED_RES, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(OLED_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

// 初始化显示屏
esp_err_t ssd1322_init(void)
{
    esp_err_t ret;

    // 初始化GPIO
    ssd1322_gpio_init();

    // 初始化SPI
    ret = ssd1322_spi_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // 硬件复位
    ssd1322_reset();

    // 初始化序列
    ssd1322_command(0xFD); // SET COMMAND LOCK
    ssd1322_data(0x12);    // UNLOCK

    ssd1322_command(0xAE); // DISPLAY OFF

    ssd1322_command(0xB3); // DISPLAY DIVIDE CLOCKRATIO/OSCILLATOR FREQUENCY
    ssd1322_data(0x91);

    ssd1322_command(0xCA); // multiplex ratio
    ssd1322_data(0x3F);    // duty = 1/64

    ssd1322_command(0xA2); // set offset
    ssd1322_data(0x00);

    ssd1322_command(0xA1); // start line
    ssd1322_data(0x00);

    ssd1322_command(0xA0); // set remap
    ssd1322_data(0x14);
    ssd1322_data(0x11);

    ssd1322_command(0xAB); // function selection
    ssd1322_data(0x01);    // selection external vdd

    ssd1322_command(0xB4);
    ssd1322_data(0xA0);
    ssd1322_data(0xFD);

    ssd1322_command(0xC1); // set contrast current
    ssd1322_data(0x80);

    ssd1322_command(0xC7); // master contrast current control
    ssd1322_data(0x0F);

    ssd1322_command(0xB1); // SET PHASE LENGTH
    ssd1322_data(0xE2);

    ssd1322_command(0xD1);
    ssd1322_data(0x82);
    ssd1322_data(0x20);

    ssd1322_command(0xBB); // SET PRE-CHARGE VOLTAGE
    ssd1322_data(0x1F);

    ssd1322_command(0xB6); // SET SECOND PRE-CHARGE PERIOD
    ssd1322_data(0x08);

    ssd1322_command(0xBE); // SET VCOMH
    ssd1322_data(0x07);

    ssd1322_command(0xA6); // normal display

    ssd1322_command(0xAF); // display ON

    ESP_LOGI(TAG, "SSD1322 initialized successfully");
    return ESP_OK;
}

// 设置显示窗口
void ssd1322_set_window(uint8_t x_start, uint8_t y_start, uint8_t x_end, uint8_t y_end)
{
    ssd1322_command(0x15);
    ssd1322_data(x_start + 0x1C);
    ssd1322_data(x_end + 0x1C);
    ssd1322_command(0x75);
    ssd1322_data(y_start);
    ssd1322_data(y_end);
    ssd1322_command(0x5C); // write ram command
}

// 清除屏幕（上半部分）
void ssd1322_clear(void)
{
    ssd1322_set_window(0, 0, 63, 63);
    ssd1322_start_data();
    for (int i = 0; i < 8192; i++) {
        ssd1322_send_data(0x00);
    }
    ssd1322_end_data();
}

// 清除整个屏幕
void ssd1322_clear_all(void)
{
    ssd1322_set_window(0, 0, 63, 127);
    ssd1322_start_data();
    for (int i = 0; i < 16384; i++) {
        ssd1322_send_data(0x00);
    }
    ssd1322_end_data();
}

// 数据处理函数
static void ssd1322_data_processing(uint8_t temp)
{
    ssd1322_send_data(((temp & 0x80) ? 0xF0 : 0x00) | ((temp & 0x40) ? 0x0F : 0x00)); // Pixel1,Pixel2
    ssd1322_send_data(((temp & 0x20) ? 0xF0 : 0x00) | ((temp & 0x10) ? 0x0F : 0x00)); // Pixel3,Pixel4
    ssd1322_send_data(((temp & 0x08) ? 0xF0 : 0x00) | ((temp & 0x04) ? 0x0F : 0x00)); // Pixel5,Pixel6
    ssd1322_send_data(((temp & 0x02) ? 0xF0 : 0x00) | ((temp & 0x01) ? 0x0F : 0x00)); // Pixel7,Pixel8
}

// 显示单个字符
void ssd1322_char(uint8_t x, uint8_t y, char ch, uint8_t mode)
{
    x = x / 4;
    int offset = (ch - 32) * 15 + 4;
    ssd1322_set_window(x, y, x + 1, y + 14);
    ssd1322_start_data();
    for (int i = 0; i < 15; i++) {
        uint8_t str = font9x15[offset + i];
        if (mode) {
            str = ~str;
        }
        ssd1322_data_processing(str);
    }
    ssd1322_end_data();
}

// 显示字符串
void ssd1322_string(uint8_t x, uint8_t y, const char *str, uint8_t mode)
{
    while (*str != 0) {
        ssd1322_char(x, y, *str, mode);
        x += 8; // 字体宽度
        str++;
    }
}

// 显示单色位图
void ssd1322_bitmap_mono(const uint8_t *bitmap)
{
    ssd1322_set_window(0, 0, 255 / 4, 63);
    ssd1322_start_data();
    for (int row = 0; row < 64; row++) {
        for (int col = 0; col < 256 / 8; col++) {
            uint8_t dat = *bitmap++;
            ssd1322_data_processing(dat);
        }
    }
    ssd1322_end_data();
}

// 显示灰度位图
void ssd1322_bitmap_gray(const uint8_t *bitmap)
{
    ssd1322_set_window(0, 0, 255 / 4, 63);
    ssd1322_start_data();
    for (int row = 0; row < 64; row++) {
        for (int col = 0; col < 128; col++) {
            ssd1322_send_data(*bitmap++);
        }
    }
    ssd1322_end_data();
}
