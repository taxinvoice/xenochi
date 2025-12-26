# Sound assets: beep_8k_mono.c, weee_hahaha_16k.c, sound_braking.c, sound_face_down.c, sound_low_battery.c
file(GLOB_RECURSE SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/*.c"
"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "include" "assets"
    REQUIRES esp-brookesia lvgl__lvgl bsp_esp32_c6_touch_lcd_1_83 sd_file audio_play wifi_manager esp_http_client json nvs_flash power_manager)
