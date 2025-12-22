/**
 * @file wifi_manager.h
 * @brief WiFi Manager for MiBuddy - Handles automatic WiFi connection and status monitoring
 *
 * This module provides:
 * - Automatic WiFi connection on boot using Kconfig credentials
 * - Status callbacks for UI updates (connection state, signal strength)
 * - Automatic reconnection on disconnect (infinite retry)
 * - Thread-safe status queries
 *
 * Usage:
 *   1. Configure SSID/password via `idf.py menuconfig` -> MiBuddy WiFi Configuration
 *   2. Call wifi_manager_init() with a status callback
 *   3. The callback will be invoked on connect/disconnect with RSSI info
 *
 * @note This module is designed to be independent of the UI framework.
 *       The status callback can be used to update LVGL, log messages, etc.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_wifi_types.h"  /* For wifi_ap_record_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi status callback type
 *
 * Called when WiFi connection status changes (connected/disconnected)
 *
 * @param connected true if connected to WiFi, false if disconnected
 * @param rssi Signal strength in dBm (only valid when connected)
 *             Typical values: -30 (excellent) to -90 (very weak)
 */
typedef void (*wifi_status_callback_t)(bool connected, int rssi);

/**
 * @brief Initialize the WiFi manager and start connection
 *
 * This function:
 * - Initializes the ESP-IDF WiFi stack
 * - Configures STA mode with credentials from Kconfig
 * - Starts the connection process
 * - Registers event handlers for status monitoring
 *
 * The status callback will be invoked:
 * - When connection is established (with RSSI)
 * - When connection is lost (rssi = 0)
 * - On reconnection after temporary disconnect
 *
 * @param status_cb Callback function for status updates (can be NULL)
 *
 * @note Call this early in app_main(), before UI setup if possible
 * @note WiFi credentials are read from CONFIG_MIBUDDY_WIFI_SSID and CONFIG_MIBUDDY_WIFI_PASSWORD
 */
void wifi_manager_init(wifi_status_callback_t status_cb);

/**
 * @brief Check if WiFi is currently connected
 *
 * Thread-safe query of connection status.
 *
 * @return true if connected to an access point
 * @return false if disconnected or connecting
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get current signal strength (RSSI)
 *
 * @return RSSI in dBm if connected, 0 if not connected
 *
 * RSSI interpretation:
 * - > -50 dBm: Excellent signal
 * - -50 to -60 dBm: Good signal
 * - -60 to -70 dBm: Fair signal
 * - -70 to -80 dBm: Weak signal
 * - < -80 dBm: Very weak signal
 */
int wifi_manager_get_rssi(void);

/**
 * @brief Get current IP address as string
 *
 * @param ip_buf Buffer to store the IP string (e.g., "192.168.1.100")
 * @param buf_len Size of the buffer (recommend at least 16 bytes)
 *
 * @note Returns "0.0.0.0" if not connected
 */
void wifi_manager_get_ip(char *ip_buf, size_t buf_len);

/**
 * @brief Get the SSID of the currently connected network
 *
 * @param ssid_buf Buffer to store the SSID string
 * @param buf_len Size of the buffer (max SSID length is 32 bytes)
 *
 * @note Returns empty string if not connected
 */
void wifi_manager_get_ssid(char *ssid_buf, size_t buf_len);

/**
 * @brief Trigger a manual reconnection attempt
 *
 * Useful if you want to force a reconnect after changing credentials
 * or if the automatic retry is taking too long.
 */
void wifi_manager_reconnect(void);

/**
 * @brief Scan for available WiFi networks
 *
 * Performs a WiFi scan and returns the list of available access points.
 * This is a blocking call that waits for scan completion.
 *
 * @param ap_info Array to store scan results
 * @param ap_count Pointer to store number of APs found (also max to return)
 * @param max_aps Maximum number of APs to return
 * @param timeout_ms Timeout in milliseconds to wait for scan
 *
 * @return true if scan completed successfully, false on timeout or error
 *
 * @note The scan temporarily interrupts the connection but auto-reconnects
 */
bool wifi_manager_scan(wifi_ap_record_t *ap_info, uint16_t *ap_count, uint16_t max_aps, uint32_t timeout_ms);

/**
 * @brief Deinitialize WiFi manager
 *
 * Disconnects from WiFi and releases all resources.
 * Call this only if you need to completely shut down WiFi.
 *
 * @note After calling this, wifi_manager_init() must be called again to use WiFi
 */
void wifi_manager_deinit(void);

#ifdef __cplusplus
}
#endif
