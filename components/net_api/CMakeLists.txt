idf_component_register(
    SRCS "net_api.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls wifi_manager
    PRIV_REQUIRES esp_event freertos
)
