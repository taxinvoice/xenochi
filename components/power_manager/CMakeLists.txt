idf_component_register(
    SRCS "power_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_esp32_c6_touch_lcd_1_83 app_mibuddy nvs_flash driver esp_timer
)
