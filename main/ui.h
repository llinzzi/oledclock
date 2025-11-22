#ifndef UI_H
#define UI_H

#include "esp_err.h"

/**
 * @brief 初始化并创建UI界面
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t ui_init(void);

#endif // UI_H
