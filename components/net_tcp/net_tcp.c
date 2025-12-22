/**
 * @file net_tcp.c
 * @brief Raw TCP Socket STUB implementation
 *
 * @todo Implement using lwip sockets and esp-tls
 */

#include "net_tcp.h"
#include "esp_log.h"

static const char *TAG = "net_tcp";

net_tcp_handle_t net_tcp_connect(const char *host, uint16_t port, bool use_tls)
{
    ESP_LOGW(TAG, "STUB: net_tcp_connect() not implemented");
    (void)host;
    (void)port;
    (void)use_tls;
    return NULL;
}

int net_tcp_send(net_tcp_handle_t handle, const void *data, size_t len)
{
    ESP_LOGW(TAG, "STUB: net_tcp_send() not implemented");
    (void)handle;
    (void)data;
    (void)len;
    return -1;
}

int net_tcp_recv(net_tcp_handle_t handle, void *buffer, size_t max_len, int timeout_ms)
{
    ESP_LOGW(TAG, "STUB: net_tcp_recv() not implemented");
    (void)handle;
    (void)buffer;
    (void)max_len;
    (void)timeout_ms;
    return -1;
}

bool net_tcp_is_connected(net_tcp_handle_t handle)
{
    (void)handle;
    return false;
}

void net_tcp_close(net_tcp_handle_t handle)
{
    ESP_LOGW(TAG, "STUB: net_tcp_close() not implemented");
    (void)handle;
}
