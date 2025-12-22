# 初始化源文件和包含目录
set(SOURCES
    "./main.cpp" 
    "./240_284/dark/main_bg_240_284.c"
)

set(INCLUDE_DIRS 
    "."
    "./240_284/dark"
)


# 注册组件
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES
        wifi_manager
        nvs_flash
        bsp_esp32_c6_touch_lcd_1_83
        esp-brookesia
        app_music
        app_setting
        app_gyroscope
        app_rec_test
        audio_play
)