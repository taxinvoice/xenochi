/**
 * @file net_mqtt.h
 * @brief MQTT Client wrapper for ESP32 - Simplified interface for esp-mqtt
 *
 * This module provides:
 * - MQTT connection management with automatic reconnection
 * - Publish/subscribe with QoS support
 * - SSL/TLS support for secure connections
 * - Event callbacks for connection status and messages
 *
 * @note This is a STUB implementation - functions return ESP_ERR_NOT_SUPPORTED
 * @todo Implement using ESP-IDF mqtt component
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback for received MQTT messages
 *
 * @param topic Topic the message was received on (null-terminated)
 * @param data Message payload
 * @param len Length of message payload
 */
typedef void (*net_mqtt_msg_cb_t)(const char *topic, const void *data, size_t len);

/**
 * @brief Callback for MQTT connection events
 *
 * @param connected true if connected, false if disconnected
 */
typedef void (*net_mqtt_event_cb_t)(bool connected);

/**
 * @brief Initialize MQTT client with broker URI
 *
 * @param broker_uri MQTT broker URI (mqtt://, mqtts://, ws://, wss://)
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_mqtt_init(const char *broker_uri);

/**
 * @brief Set authentication credentials
 *
 * @param username MQTT username (can be NULL)
 * @param password MQTT password (can be NULL)
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_mqtt_set_credentials(const char *username, const char *password);

/**
 * @brief Connect to the MQTT broker
 *
 * @param event_cb Callback for connection status changes
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_mqtt_connect(net_mqtt_event_cb_t event_cb);

/**
 * @brief Publish a message to a topic
 *
 * @param topic Topic to publish to
 * @param data Message payload
 * @param len Length of payload (0 for null-terminated string)
 * @param qos Quality of Service (0, 1, or 2)
 * @param retain Retain flag
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_mqtt_publish(const char *topic, const void *data, size_t len, int qos, bool retain);

/**
 * @brief Subscribe to a topic
 *
 * @param topic Topic to subscribe to (supports wildcards +, #)
 * @param qos Quality of Service for subscription
 * @param callback Callback for messages received on this topic
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_mqtt_subscribe(const char *topic, int qos, net_mqtt_msg_cb_t callback);

/**
 * @brief Unsubscribe from a topic
 *
 * @param topic Topic to unsubscribe from
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED (stub)
 */
esp_err_t net_mqtt_unsubscribe(const char *topic);

/**
 * @brief Check if connected to broker
 *
 * @return true if connected, false otherwise
 */
bool net_mqtt_is_connected(void);

/**
 * @brief Disconnect from broker
 */
void net_mqtt_disconnect(void);

/**
 * @brief Deinitialize MQTT client
 */
void net_mqtt_deinit(void);

#ifdef __cplusplus
}
#endif
