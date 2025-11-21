#pragma once

#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SSD1322 panel configuration structure
 */
typedef struct {
    uint8_t height;         ///< Panel height in pixels (default 64)
    uint8_t width;          ///< Panel width in pixels (default 256)
} esp_lcd_ssd1322_config_t;

/**
 * @brief Create LCD panel for SSD1322 driver
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config General panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if parameter is invalid
 *      - ESP_ERR_NO_MEM if out of memory
 */
esp_err_t esp_lcd_new_panel_ssd1322(const esp_lcd_panel_io_handle_t io,
                                     const esp_lcd_panel_dev_config_t *panel_dev_config,
                                     esp_lcd_panel_handle_t *ret_panel);

#ifdef __cplusplus
}
#endif
