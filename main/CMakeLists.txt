idf_component_register(SRCS "main.c"
                            "ssd1322_driver.c"
                            "lvgl_adapter.c"
                            "ui.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl__lvgl driver)