# 初始化源文件和包含目录
set(SOURCES
    "./main.cpp"
    "./240_284/dark/main_bg_240_284.c"
    # Embedded assets
    "./assets/sounds/beep_8k_mono.c"
    "./assets/images/icon_sample_16x16.c"
    # MochiState asset configuration
    "./mochi_asset_setup.c"
)

set(INCLUDE_DIRS
    "."
    "./240_284/dark"
    "./assets"
)


# 注册组件
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES
        wifi_manager
        time_sync
        sd_logger
        nvs_flash
        bsp_esp32_c6_touch_lcd_1_83
        esp-brookesia
        app_music
        app_setting
        app_gyroscope
        app_rec_test
        app_mibuddy
        audio_play
        net_api
        net_mqtt
        net_tcp
        net_file
)