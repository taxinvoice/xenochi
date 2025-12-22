/**
 * @file net_mqtt.c
 * @brief MQTT Client STUB implementation
 *
 * @todo Implement using ESP-IDF mqtt component
 */

#include "net_mqtt.h"
#include "esp_log.h"

static const char *TAG = "net_mqtt";

esp_err_t net_mqtt_init(const char *broker_uri)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_init() not implemented");
    (void)broker_uri;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_mqtt_set_credentials(const char *username, const char *password)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_set_credentials() not implemented");
    (void)username;
    (void)password;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_mqtt_connect(net_mqtt_event_cb_t event_cb)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_connect() not implemented");
    (void)event_cb;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_mqtt_publish(const char *topic, const void *data, size_t len, int qos, bool retain)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_publish() not implemented");
    (void)topic;
    (void)data;
    (void)len;
    (void)qos;
    (void)retain;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_mqtt_subscribe(const char *topic, int qos, net_mqtt_msg_cb_t callback)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_subscribe() not implemented");
    (void)topic;
    (void)qos;
    (void)callback;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t net_mqtt_unsubscribe(const char *topic)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_unsubscribe() not implemented");
    (void)topic;
    return ESP_ERR_NOT_SUPPORTED;
}

bool net_mqtt_is_connected(void)
{
    return false;
}

void net_mqtt_disconnect(void)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_disconnect() not implemented");
}

void net_mqtt_deinit(void)
{
    ESP_LOGW(TAG, "STUB: net_mqtt_deinit() not implemented");
}
