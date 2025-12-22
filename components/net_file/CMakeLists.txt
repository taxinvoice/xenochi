idf_component_register(
    SRCS "net_file.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client sd_file esp-tls
)
