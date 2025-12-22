/**
 * @file net_api.h
 * @brief HTTP REST API Client for ESP32 - Simplified wrapper for esp_http_client
 *
 * This module provides:
 * - Synchronous HTTP methods (GET, POST, PUT, DELETE)
 * - Asynchronous HTTP methods with callbacks
 * - SSL/TLS support via ESP certificate bundle
 * - Custom header management
 * - Bearer token authentication
 *
 * Usage:
 *   1. Call net_api_init() once at startup (after WiFi is connected)
 *   2. Use net_api_get/post/put/delete for sync requests
 *   3. Use net_api_*_async for non-blocking requests with callbacks
 *   4. Always call net_api_free_response() after processing response
 *
 * @note Requires WiFi connection before making requests
 * @note SSL/TLS is automatically enabled for https:// URLs
 */

#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HTTP response structure
 *
 * Contains the result of an HTTP request including status code and body.
 * The body is dynamically allocated and must be freed with net_api_free_response().
 */
typedef struct {
    int status_code;      /**< HTTP status code (200, 404, 500, etc.) or -1 on error */
    char *body;           /**< Response body (null-terminated string, may be NULL) */
    size_t body_len;      /**< Length of response body in bytes */
    esp_err_t err;        /**< ESP error code if request failed */
} net_api_response_t;

/**
 * @brief Async request callback type
 *
 * Called when an async HTTP request completes (success or failure).
 * The response is automatically freed after the callback returns.
 *
 * @param response Pointer to response structure (valid only during callback)
 * @param user_data User data pointer passed to the async function
 */
typedef void (*net_api_callback_t)(const net_api_response_t *response, void *user_data);

/**
 * @brief Initialize the net_api module
 *
 * Must be called once before any HTTP requests.
 * Can be called before WiFi is connected, but requests will fail until connected.
 *
 * @return ESP_OK on success
 */
esp_err_t net_api_init(void);

/**
 * @brief Deinitialize the net_api module
 *
 * Clears all custom headers and releases resources.
 * Any pending async requests will complete before returning.
 */
void net_api_deinit(void);

/* ============================================================================
 * Synchronous HTTP Methods (Blocking)
 * ============================================================================ */

/**
 * @brief Perform a synchronous HTTP GET request
 *
 * @param url Full URL to request (http:// or https://)
 * @param response Pointer to response structure to populate
 * @return ESP_OK on success, error code on failure
 *
 * @note Blocks until request completes or times out
 * @note Call net_api_free_response() after processing
 */
esp_err_t net_api_get(const char *url, net_api_response_t *response);

/**
 * @brief Perform a synchronous HTTP POST request
 *
 * @param url Full URL to request
 * @param body Request body (can be NULL for empty body)
 * @param content_type Content-Type header value (e.g., "application/json")
 * @param response Pointer to response structure to populate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t net_api_post(const char *url, const char *body, const char *content_type,
                       net_api_response_t *response);

/**
 * @brief Perform a synchronous HTTP PUT request
 *
 * @param url Full URL to request
 * @param body Request body (can be NULL for empty body)
 * @param content_type Content-Type header value
 * @param response Pointer to response structure to populate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t net_api_put(const char *url, const char *body, const char *content_type,
                      net_api_response_t *response);

/**
 * @brief Perform a synchronous HTTP DELETE request
 *
 * @param url Full URL to request
 * @param response Pointer to response structure to populate
 * @return ESP_OK on success, error code on failure
 */
esp_err_t net_api_delete(const char *url, net_api_response_t *response);

/* ============================================================================
 * Asynchronous HTTP Methods (Non-Blocking)
 * ============================================================================ */

/**
 * @brief Perform an asynchronous HTTP GET request
 *
 * Returns immediately, callback is invoked when request completes.
 *
 * @param url Full URL to request
 * @param callback Function to call when request completes
 * @param user_data User data to pass to callback
 * @return ESP_OK if request was started, error code on failure
 */
esp_err_t net_api_get_async(const char *url, net_api_callback_t callback, void *user_data);

/**
 * @brief Perform an asynchronous HTTP POST request
 *
 * @param url Full URL to request
 * @param body Request body (copied internally, can be freed after call returns)
 * @param content_type Content-Type header value
 * @param callback Function to call when request completes
 * @param user_data User data to pass to callback
 * @return ESP_OK if request was started, error code on failure
 */
esp_err_t net_api_post_async(const char *url, const char *body, const char *content_type,
                             net_api_callback_t callback, void *user_data);

/**
 * @brief Perform an asynchronous HTTP PUT request
 *
 * @param url Full URL to request
 * @param body Request body (copied internally)
 * @param content_type Content-Type header value
 * @param callback Function to call when request completes
 * @param user_data User data to pass to callback
 * @return ESP_OK if request was started, error code on failure
 */
esp_err_t net_api_put_async(const char *url, const char *body, const char *content_type,
                            net_api_callback_t callback, void *user_data);

/**
 * @brief Perform an asynchronous HTTP DELETE request
 *
 * @param url Full URL to request
 * @param callback Function to call when request completes
 * @param user_data User data to pass to callback
 * @return ESP_OK if request was started, error code on failure
 */
esp_err_t net_api_delete_async(const char *url, net_api_callback_t callback, void *user_data);

/* ============================================================================
 * Header Management
 * ============================================================================ */

/**
 * @brief Set a custom HTTP header for all subsequent requests
 *
 * Headers persist until cleared or module is deinitialized.
 *
 * @param key Header name (e.g., "X-API-Key")
 * @param value Header value
 * @return ESP_OK on success, ESP_ERR_NO_MEM if max headers reached
 */
esp_err_t net_api_set_header(const char *key, const char *value);

/**
 * @brief Set Bearer token for Authorization header
 *
 * Convenience function that sets "Authorization: Bearer <token>"
 *
 * @param token The bearer token (without "Bearer " prefix)
 */
void net_api_set_bearer_token(const char *token);

/**
 * @brief Clear all custom headers
 */
void net_api_clear_headers(void);

/* ============================================================================
 * Response Management
 * ============================================================================ */

/**
 * @brief Free memory allocated for response body
 *
 * Must be called after processing a synchronous response.
 * Async responses are automatically freed after callback returns.
 *
 * @param response Pointer to response structure
 */
void net_api_free_response(net_api_response_t *response);

#ifdef __cplusplus
}
#endif
