/**
 * @file net_api.c
 * @brief HTTP REST API Client implementation using esp_http_client
 */

#include "net_api.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "net_api";

/* ============================================================================
 * Configuration
 * ============================================================================ */

#ifndef CONFIG_NET_API_TIMEOUT_MS
#define CONFIG_NET_API_TIMEOUT_MS 10000
#endif

#ifndef CONFIG_NET_API_MAX_RESPONSE_SIZE
#define CONFIG_NET_API_MAX_RESPONSE_SIZE 8192
#endif

#ifndef CONFIG_NET_API_TASK_STACK_SIZE
#define CONFIG_NET_API_TASK_STACK_SIZE 8192
#endif

#ifndef CONFIG_NET_API_MAX_HEADERS
#define CONFIG_NET_API_MAX_HEADERS 8
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

typedef struct {
    char *key;
    char *value;
} header_entry_t;

typedef struct {
    char *url;
    char *body;
    char *content_type;
    esp_http_client_method_t method;
    net_api_callback_t callback;
    void *user_data;
} async_request_t;

/* ============================================================================
 * Static Variables
 * ============================================================================ */

static bool s_initialized = false;
static header_entry_t s_headers[CONFIG_NET_API_MAX_HEADERS];
static int s_header_count = 0;
static SemaphoreHandle_t s_header_mutex = NULL;
static char *s_bearer_token = NULL;

/* ============================================================================
 * HTTP Event Handler
 * ============================================================================ */

typedef struct {
    char *buffer;
    size_t buffer_len;
    size_t data_len;
} http_receive_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_receive_buffer_t *recv_buf = (http_receive_buffer_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (recv_buf && recv_buf->buffer) {
                size_t copy_len = evt->data_len;
                if (recv_buf->data_len + copy_len > recv_buf->buffer_len - 1) {
                    copy_len = recv_buf->buffer_len - 1 - recv_buf->data_len;
                    ESP_LOGW(TAG, "Response truncated, buffer full");
                }
                if (copy_len > 0) {
                    memcpy(recv_buf->buffer + recv_buf->data_len, evt->data, copy_len);
                    recv_buf->data_len += copy_len;
                    recv_buf->buffer[recv_buf->data_len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADERS_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADERS_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "Header: %s: %s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static void apply_custom_headers(esp_http_client_handle_t client)
{
    if (s_header_mutex) {
        xSemaphoreTake(s_header_mutex, portMAX_DELAY);
    }

    // Apply bearer token if set
    if (s_bearer_token) {
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_bearer_token);
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    // Apply custom headers
    for (int i = 0; i < s_header_count; i++) {
        if (s_headers[i].key && s_headers[i].value) {
            esp_http_client_set_header(client, s_headers[i].key, s_headers[i].value);
        }
    }

    if (s_header_mutex) {
        xSemaphoreGive(s_header_mutex);
    }
}

static esp_err_t perform_request(const char *url, esp_http_client_method_t method,
                                  const char *body, const char *content_type,
                                  net_api_response_t *response)
{
    if (!url || !response) {
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize response
    memset(response, 0, sizeof(net_api_response_t));
    response->status_code = -1;
    response->err = ESP_OK;

    // Allocate receive buffer
    http_receive_buffer_t recv_buf = {
        .buffer = calloc(1, CONFIG_NET_API_MAX_RESPONSE_SIZE),
        .buffer_len = CONFIG_NET_API_MAX_RESPONSE_SIZE,
        .data_len = 0
    };

    if (!recv_buf.buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        response->err = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }

    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = CONFIG_NET_API_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .user_data = &recv_buf,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Enable SSL cert validation
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(recv_buf.buffer);
        response->err = ESP_FAIL;
        return ESP_FAIL;
    }

    // Apply custom headers
    apply_custom_headers(client);

    // Set content type and body for POST/PUT
    if (content_type) {
        esp_http_client_set_header(client, "Content-Type", content_type);
    }
    if (body && (method == HTTP_METHOD_POST || method == HTTP_METHOD_PUT)) {
        esp_http_client_set_post_field(client, body, strlen(body));
    }

    // Perform the request
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        response->status_code = esp_http_client_get_status_code(client);
        response->body_len = recv_buf.data_len;
        response->body = recv_buf.buffer;  // Transfer ownership
        ESP_LOGI(TAG, "HTTP %s %s -> %d (%zu bytes)",
                 method == HTTP_METHOD_GET ? "GET" :
                 method == HTTP_METHOD_POST ? "POST" :
                 method == HTTP_METHOD_PUT ? "PUT" :
                 method == HTTP_METHOD_DELETE ? "DELETE" : "?",
                 url, response->status_code, response->body_len);
    } else {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        free(recv_buf.buffer);
        response->err = err;
    }

    esp_http_client_cleanup(client);
    return err;
}

/* ============================================================================
 * Async Task
 * ============================================================================ */

static void async_request_task(void *arg)
{
    async_request_t *req = (async_request_t *)arg;
    net_api_response_t response;

    // Perform the request
    perform_request(req->url, req->method, req->body, req->content_type, &response);

    // Invoke callback
    if (req->callback) {
        req->callback(&response, req->user_data);
    }

    // Cleanup
    net_api_free_response(&response);
    free(req->url);
    free(req->body);
    free(req->content_type);
    free(req);

    vTaskDelete(NULL);
}

static esp_err_t start_async_request(const char *url, esp_http_client_method_t method,
                                      const char *body, const char *content_type,
                                      net_api_callback_t callback, void *user_data)
{
    if (!url) {
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate request structure
    async_request_t *req = calloc(1, sizeof(async_request_t));
    if (!req) {
        return ESP_ERR_NO_MEM;
    }

    // Copy strings (they may be freed by caller before task runs)
    req->url = strdup(url);
    req->body = body ? strdup(body) : NULL;
    req->content_type = content_type ? strdup(content_type) : NULL;
    req->method = method;
    req->callback = callback;
    req->user_data = user_data;

    if (!req->url || (body && !req->body) || (content_type && !req->content_type)) {
        free(req->url);
        free(req->body);
        free(req->content_type);
        free(req);
        return ESP_ERR_NO_MEM;
    }

    // Create task
    BaseType_t ret = xTaskCreate(async_request_task, "net_api_async",
                                  CONFIG_NET_API_TASK_STACK_SIZE,
                                  req, 5, NULL);
    if (ret != pdPASS) {
        free(req->url);
        free(req->body);
        free(req->content_type);
        free(req);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

esp_err_t net_api_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_header_mutex = xSemaphoreCreateMutex();
    if (!s_header_mutex) {
        ESP_LOGE(TAG, "Failed to create header mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(s_headers, 0, sizeof(s_headers));
    s_header_count = 0;
    s_bearer_token = NULL;
    s_initialized = true;

    ESP_LOGI(TAG, "Initialized (timeout=%dms, max_response=%d bytes)",
             CONFIG_NET_API_TIMEOUT_MS, CONFIG_NET_API_MAX_RESPONSE_SIZE);

    return ESP_OK;
}

void net_api_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    net_api_clear_headers();

    if (s_bearer_token) {
        free(s_bearer_token);
        s_bearer_token = NULL;
    }

    if (s_header_mutex) {
        vSemaphoreDelete(s_header_mutex);
        s_header_mutex = NULL;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "Deinitialized");
}

/* Synchronous methods */

esp_err_t net_api_get(const char *url, net_api_response_t *response)
{
    return perform_request(url, HTTP_METHOD_GET, NULL, NULL, response);
}

esp_err_t net_api_post(const char *url, const char *body, const char *content_type,
                       net_api_response_t *response)
{
    return perform_request(url, HTTP_METHOD_POST, body, content_type, response);
}

esp_err_t net_api_put(const char *url, const char *body, const char *content_type,
                      net_api_response_t *response)
{
    return perform_request(url, HTTP_METHOD_PUT, body, content_type, response);
}

esp_err_t net_api_delete(const char *url, net_api_response_t *response)
{
    return perform_request(url, HTTP_METHOD_DELETE, NULL, NULL, response);
}

/* Asynchronous methods */

esp_err_t net_api_get_async(const char *url, net_api_callback_t callback, void *user_data)
{
    return start_async_request(url, HTTP_METHOD_GET, NULL, NULL, callback, user_data);
}

esp_err_t net_api_post_async(const char *url, const char *body, const char *content_type,
                             net_api_callback_t callback, void *user_data)
{
    return start_async_request(url, HTTP_METHOD_POST, body, content_type, callback, user_data);
}

esp_err_t net_api_put_async(const char *url, const char *body, const char *content_type,
                            net_api_callback_t callback, void *user_data)
{
    return start_async_request(url, HTTP_METHOD_PUT, body, content_type, callback, user_data);
}

esp_err_t net_api_delete_async(const char *url, net_api_callback_t callback, void *user_data)
{
    return start_async_request(url, HTTP_METHOD_DELETE, NULL, NULL, callback, user_data);
}

/* Header management */

esp_err_t net_api_set_header(const char *key, const char *value)
{
    if (!key || !value) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_header_mutex) {
        xSemaphoreTake(s_header_mutex, portMAX_DELAY);
    }

    // Check if header already exists
    for (int i = 0; i < s_header_count; i++) {
        if (s_headers[i].key && strcmp(s_headers[i].key, key) == 0) {
            // Update existing header
            free(s_headers[i].value);
            s_headers[i].value = strdup(value);
            if (s_header_mutex) {
                xSemaphoreGive(s_header_mutex);
            }
            return s_headers[i].value ? ESP_OK : ESP_ERR_NO_MEM;
        }
    }

    // Add new header
    if (s_header_count >= CONFIG_NET_API_MAX_HEADERS) {
        if (s_header_mutex) {
            xSemaphoreGive(s_header_mutex);
        }
        ESP_LOGE(TAG, "Max headers reached (%d)", CONFIG_NET_API_MAX_HEADERS);
        return ESP_ERR_NO_MEM;
    }

    s_headers[s_header_count].key = strdup(key);
    s_headers[s_header_count].value = strdup(value);

    if (!s_headers[s_header_count].key || !s_headers[s_header_count].value) {
        free(s_headers[s_header_count].key);
        free(s_headers[s_header_count].value);
        s_headers[s_header_count].key = NULL;
        s_headers[s_header_count].value = NULL;
        if (s_header_mutex) {
            xSemaphoreGive(s_header_mutex);
        }
        return ESP_ERR_NO_MEM;
    }

    s_header_count++;

    if (s_header_mutex) {
        xSemaphoreGive(s_header_mutex);
    }

    return ESP_OK;
}

void net_api_set_bearer_token(const char *token)
{
    if (s_header_mutex) {
        xSemaphoreTake(s_header_mutex, portMAX_DELAY);
    }

    if (s_bearer_token) {
        free(s_bearer_token);
        s_bearer_token = NULL;
    }

    if (token) {
        s_bearer_token = strdup(token);
    }

    if (s_header_mutex) {
        xSemaphoreGive(s_header_mutex);
    }
}

void net_api_clear_headers(void)
{
    if (s_header_mutex) {
        xSemaphoreTake(s_header_mutex, portMAX_DELAY);
    }

    for (int i = 0; i < s_header_count; i++) {
        free(s_headers[i].key);
        free(s_headers[i].value);
        s_headers[i].key = NULL;
        s_headers[i].value = NULL;
    }
    s_header_count = 0;

    if (s_header_mutex) {
        xSemaphoreGive(s_header_mutex);
    }
}

void net_api_free_response(net_api_response_t *response)
{
    if (response) {
        free(response->body);
        response->body = NULL;
        response->body_len = 0;
    }
}
