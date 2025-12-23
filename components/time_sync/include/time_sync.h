/**
 * @file time_sync.h
 * @brief NTP Time Synchronization for MiBuddy
 *
 * This module provides automatic time synchronization from NTP servers
 * to the system clock and hardware RTC (PCF85063A).
 *
 * Features:
 * - Automatic sync when WiFi connects (configurable)
 * - Timezone support via POSIX TZ strings
 * - Hardware RTC update for time persistence across reboots
 * - Periodic re-sync (handled by LWIP SNTP)
 *
 * Usage:
 *   1. Configure NTP server and timezone via `idf.py menuconfig`
 *      -> MiBuddy Time Sync Configuration
 *   2. Call time_sync_init() after wifi_manager_init()
 *   3. Time will sync automatically when WiFi connects
 *
 * @note Requires WiFi to be connected for NTP synchronization
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback invoked when time synchronization completes
 *
 * @param success true if sync succeeded, false on failure/timeout
 * @param utc_time The synchronized UTC time (seconds since epoch)
 */
typedef void (*time_sync_callback_t)(bool success, time_t utc_time);

/**
 * @brief Initialize the time sync module
 *
 * This function:
 * - Sets the timezone from Kconfig (CONFIG_MIBUDDY_TIMEZONE)
 * - Initializes the SNTP client with configured NTP server
 * - Registers for sync notifications
 *
 * When WiFi is connected, SNTP will automatically fetch time.
 * Once synced, the hardware RTC is updated.
 *
 * @param callback Optional callback for sync events (can be NULL)
 *
 * @note Call this after wifi_manager_init() in app_main()
 */
void time_sync_init(time_sync_callback_t callback);

/**
 * @brief Trigger manual time synchronization
 *
 * Forces an immediate time sync request. Useful after timezone change
 * or if automatic sync needs to be expedited.
 *
 * @return true if sync request was initiated
 * @return false if sync already in progress or WiFi not connected
 */
bool time_sync_now(void);

/**
 * @brief Check if time has been synchronized at least once
 *
 * Use this to determine if the system time is valid.
 *
 * @return true if time has been synced from NTP
 * @return false if time is still the default/unsynced
 */
bool time_sync_is_synced(void);

/**
 * @brief Get current local time as formatted string
 *
 * @param buf Buffer to store the formatted time string
 * @param buf_len Size of the buffer
 * @param format strftime format string (e.g., "%Y-%m-%d %H:%M:%S")
 *
 * @note Returns "Time not synced" if time hasn't been synchronized yet
 */
void time_sync_get_time_str(char *buf, size_t buf_len, const char *format);

/**
 * @brief Update the hardware RTC with current system time
 *
 * Writes the current system time to the PCF85063A RTC chip.
 * This is called automatically after NTP sync, but can be
 * called manually if needed.
 *
 * @return ESP_OK on success
 * @return ESP_FAIL if RTC write failed
 */
esp_err_t time_sync_update_rtc(void);

/**
 * @brief Load time from hardware RTC into system clock
 *
 * Reads the PCF85063A RTC and sets the system clock.
 * Useful on boot before WiFi is available.
 *
 * @return ESP_OK on success
 * @return ESP_FAIL if RTC read failed
 */
esp_err_t time_sync_load_from_rtc(void);

/**
 * @brief Get the timestamp of the last successful NTP sync
 *
 * This value is persisted in NVS across reboots.
 *
 * @return Unix timestamp of last NTP sync, or 0 if never synced
 */
time_t time_sync_get_last_ntp_time(void);

/**
 * @brief Get the last NTP sync time as a formatted string
 *
 * Returns the last successful NTP sync time in "YYYY-MM-DD HH:MM:SS" format,
 * or "Never" if no sync has occurred.
 *
 * @param buf Buffer to store the formatted string
 * @param buf_len Size of the buffer
 */
void time_sync_get_last_ntp_str(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif
