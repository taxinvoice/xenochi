/**
 * @file time_sync.c
 * @brief NTP Time Synchronization implementation for MiBuddy
 *
 * Code Flow:
 * 1. time_sync_init() is called from app_main() after wifi_manager_init()
 *    - Sets timezone from Kconfig (CONFIG_MIBUDDY_TIMEZONE)
 *    - Initializes SNTP client with NTP server from Kconfig
 *    - Optionally loads time from RTC as initial system time
 *
 * 2. When WiFi connects, SNTP automatically fetches time from NTP server
 *
 * 3. time_sync_notification_cb() is called when time is synchronized:
 *    - Updates the hardware RTC (PCF85063A) with new time
 *    - Invokes user callback if registered
 *    - Sets s_is_synced flag
 *
 * 4. Status bar clock and apps now display correct time
 *
 * Thread Safety:
 * - SNTP callback runs in LWIP context
 * - s_is_synced is volatile for thread-safe reads
 * - RTC operations use I2C which is thread-safe in ESP-IDF
 */

#include "time_sync.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "bsp_board.h"
#include "pcf85063a.h"
#include "lwip/ip_addr.h"
#include <string.h>
#include <sys/time.h>

static const char *TAG = "time_sync";

/* Module state */
static time_sync_callback_t s_callback = NULL;
static volatile bool s_is_synced = false;

/**
 * @brief Convert SNTP sync status to string for logging
 */
static const char* sntp_sync_status_to_str(sntp_sync_status_t status)
{
    switch (status) {
        case SNTP_SYNC_STATUS_RESET:       return "RESET (not synced)";
        case SNTP_SYNC_STATUS_COMPLETED:   return "COMPLETED";
        case SNTP_SYNC_STATUS_IN_PROGRESS: return "IN_PROGRESS";
        default:                           return "UNKNOWN";
    }
}

/**
 * @brief Log current network information (IP, DNS, gateway)
 */
static void log_network_info(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) {
        ESP_LOGW(TAG, "Could not get network interface");
        return;
    }

    /* Get IP info */
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "Network Info:");
        ESP_LOGI(TAG, "  IP Address:  " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "  Gateway:     " IPSTR, IP2STR(&ip_info.gw));
        ESP_LOGI(TAG, "  Netmask:     " IPSTR, IP2STR(&ip_info.netmask));
    }

    /* Get DNS info */
    esp_netif_dns_info_t dns_info;
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        ESP_LOGI(TAG, "  DNS Primary: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    }
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info) == ESP_OK) {
        if (dns_info.ip.u_addr.ip4.addr != 0) {
            ESP_LOGI(TAG, "  DNS Backup:  " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        }
    }
}

/**
 * @brief SNTP callback - invoked when time is synchronized from NTP
 */
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "NTP SYNC SUCCESSFUL!");
    ESP_LOGI(TAG, "========================================");

    s_is_synced = true;

    /* Log network info for debugging */
    log_network_info();

    /* Log the synchronized time */
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    /* Log both UTC and local time */
    struct tm utc_timeinfo;
    gmtime_r(&now, &utc_timeinfo);
    ESP_LOGI(TAG, "UTC Time:   %04d-%02d-%02d %02d:%02d:%02d",
        utc_timeinfo.tm_year + 1900, utc_timeinfo.tm_mon + 1, utc_timeinfo.tm_mday,
        utc_timeinfo.tm_hour, utc_timeinfo.tm_min, utc_timeinfo.tm_sec);
    ESP_LOGI(TAG, "Local Time: %04d-%02d-%02d %02d:%02d:%02d (TZ: %s)",
        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
        CONFIG_MIBUDDY_TIMEZONE);
    ESP_LOGI(TAG, "Unix timestamp: %lld", (long long)now);

    /* Log sync status */
    sntp_sync_status_t sync_status = sntp_get_sync_status();
    ESP_LOGI(TAG, "Sync status: %s", sntp_sync_status_to_str(sync_status));

    /* Update hardware RTC */
    ESP_LOGI(TAG, "Updating hardware RTC...");
    esp_err_t ret = time_sync_update_rtc();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update hardware RTC: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Hardware RTC updated successfully");
    }

    /* Invoke user callback */
    if (s_callback) {
        ESP_LOGD(TAG, "Invoking user callback");
        s_callback(true, now);
    }

    ESP_LOGI(TAG, "========================================");
}

void time_sync_init(time_sync_callback_t callback)
{
    printf("\n\n=== TIME_SYNC_INIT CALLED ===\n\n");  /* Debug: printf bypasses log level */
    ESP_LOGI(TAG, ">>> time_sync_init() called <<<");

    s_callback = callback;

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Initializing time sync module");
#ifdef CONFIG_MIBUDDY_NTP_SERVER
    ESP_LOGI(TAG, "Timezone: %s", CONFIG_MIBUDDY_TIMEZONE);
    ESP_LOGI(TAG, "NTP Server: %s", CONFIG_MIBUDDY_NTP_SERVER);
#else
    ESP_LOGE(TAG, "ERROR: Kconfig not configured! Run 'idf.py menuconfig'");
    ESP_LOGE(TAG, "Look for 'MiBuddy Time Sync Configuration' menu");
    return;  /* Cannot proceed without config */
#endif
    ESP_LOGI(TAG, "========================================");

    /* Set timezone from Kconfig */
    setenv("TZ", CONFIG_MIBUDDY_TIMEZONE, 1);
    tzset();
    ESP_LOGD(TAG, "Timezone environment variable set");

    /* Try to load time from RTC first (provides time before NTP sync) */
    esp_err_t rtc_ret = time_sync_load_from_rtc();
    ESP_LOGI(TAG, "RTC load result: %s", esp_err_to_name(rtc_ret));

    /* Configure and initialize SNTP */
    ESP_LOGI(TAG, "Configuring SNTP client...");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_MIBUDDY_NTP_SERVER);
    config.sync_cb = time_sync_notification_cb;

    esp_err_t ret = esp_netif_sntp_init(&config);
    ESP_LOGI(TAG, "esp_netif_sntp_init() returned: %s (0x%x)", esp_err_to_name(ret), ret);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SNTP: %s", esp_err_to_name(ret));
        return;
    }

    /* Log initial sync status */
    sntp_sync_status_t sync_status = sntp_get_sync_status();
    ESP_LOGI(TAG, "Initial SNTP sync status: %s", sntp_sync_status_to_str(sync_status));
    ESP_LOGI(TAG, "SNTP enabled: %s", esp_sntp_enabled() ? "YES" : "NO");
    ESP_LOGI(TAG, "SNTP initialized, waiting for WiFi connection...");
}

bool time_sync_now(void)
{
    /* Check if SNTP is initialized */
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "Triggering manual time sync");
        esp_sntp_restart();
        return true;
    }

    ESP_LOGW(TAG, "SNTP not initialized, cannot sync");
    return false;
}

bool time_sync_is_synced(void)
{
    return s_is_synced;
}

void time_sync_get_time_str(char *buf, size_t buf_len, const char *format)
{
    if (buf == NULL || buf_len == 0) {
        return;
    }

    if (!s_is_synced) {
        snprintf(buf, buf_len, "Time not synced");
        return;
    }

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buf, buf_len, format, &timeinfo);
}

esp_err_t time_sync_update_rtc(void)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    pcf85063a_datetime_t rtc_time = {
        .year = (uint16_t)(timeinfo.tm_year + 1900),
        .month = (uint8_t)(timeinfo.tm_mon + 1),
        .day = (uint8_t)timeinfo.tm_mday,
        .dotw = (uint8_t)timeinfo.tm_wday,
        .hour = (uint8_t)timeinfo.tm_hour,
        .min = (uint8_t)timeinfo.tm_min,
        .sec = (uint8_t)timeinfo.tm_sec
    };

    esp_err_t ret = set_rtc_time(&rtc_time);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Hardware RTC updated successfully");
    }

    return ret;
}

esp_err_t time_sync_load_from_rtc(void)
{
    pcf85063a_datetime_t rtc_time;
    get_rtc_data_to_str(&rtc_time);

    /* Validate RTC time (basic sanity check) */
    if (rtc_time.year < 2020 || rtc_time.year > 2099 ||
        rtc_time.month < 1 || rtc_time.month > 12 ||
        rtc_time.day < 1 || rtc_time.day > 31) {
        ESP_LOGW(TAG, "RTC time appears invalid, not loading");
        return ESP_FAIL;
    }

    /* Convert RTC time to struct tm */
    struct tm timeinfo = {
        .tm_year = rtc_time.year - 1900,
        .tm_mon = rtc_time.month - 1,
        .tm_mday = rtc_time.day,
        .tm_wday = rtc_time.dotw,
        .tm_hour = rtc_time.hour,
        .tm_min = rtc_time.min,
        .tm_sec = rtc_time.sec,
        .tm_isdst = -1  /* Let mktime determine DST */
    };

    /* Set system time from RTC */
    time_t t = mktime(&timeinfo);
    if (t == (time_t)-1) {
        ESP_LOGW(TAG, "Failed to convert RTC time");
        return ESP_FAIL;
    }

    struct timeval tv = {
        .tv_sec = t,
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);

    ESP_LOGI(TAG, "System time loaded from RTC: %04d-%02d-%02d %02d:%02d:%02d",
        rtc_time.year, rtc_time.month, rtc_time.day,
        rtc_time.hour, rtc_time.min, rtc_time.sec);

    return ESP_OK;
}
