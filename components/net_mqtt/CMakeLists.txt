idf_component_register(
    SRCS "net_mqtt.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp-tls wifi_manager
    PRIV_REQUIRES esp_event
)
