idf_component_register(
    SRCS "time_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif bsp_esp32_c6_touch_lcd_1_83 lwip
    PRIV_REQUIRES esp_event esp_timer
)
