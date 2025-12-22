idf_component_register(
    SRCS "sd_logger.c"
    INCLUDE_DIRS "include"
    REQUIRES sd_file bsp_esp32_c6_touch_lcd_1_83 nvs_flash
)
