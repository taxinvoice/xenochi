/**
 * @file net_tcp.h
 * @brief Raw TCP Socket Client for ESP32 - Low-level socket operations
 *
 * This module provides:
 * - TCP socket connection management
 * - Optional TLS encryption via esp-tls
 * - Blocking send/receive with timeout
 * - Connection status monitoring
 *
 * Use this for custom protocols or when you need direct socket access.
 * For HTTP, use net_api instead. For MQTT, use net_mqtt.
 *
 * @note This is a STUB implementation - functions return NULL/error
 * @todo Implement using lwip sockets and esp-tls
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque TCP connection handle
 */
typedef struct net_tcp_conn *net_tcp_handle_t;

/**
 * @brief Connect to a TCP server
 *
 * @param host Hostname or IP address
 * @param port Port number
 * @param use_tls Enable TLS encryption
 * @return Connection handle, or NULL on failure
 *
 * @note For TLS, server certificate is validated against the cert bundle
 */
net_tcp_handle_t net_tcp_connect(const char *host, uint16_t port, bool use_tls);

/**
 * @brief Send data over the connection
 *
 * @param handle Connection handle
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, or negative error code
 */
int net_tcp_send(net_tcp_handle_t handle, const void *data, size_t len);

/**
 * @brief Receive data from the connection
 *
 * @param handle Connection handle
 * @param buffer Buffer to receive data into
 * @param max_len Maximum bytes to receive
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking, -1 = infinite)
 * @return Number of bytes received, 0 on timeout, or negative error code
 */
int net_tcp_recv(net_tcp_handle_t handle, void *buffer, size_t max_len, int timeout_ms);

/**
 * @brief Check if connection is still active
 *
 * @param handle Connection handle
 * @return true if connected, false otherwise
 */
bool net_tcp_is_connected(net_tcp_handle_t handle);

/**
 * @brief Close and free a connection
 *
 * @param handle Connection handle (can be NULL)
 */
void net_tcp_close(net_tcp_handle_t handle);

#ifdef __cplusplus
}
#endif
