/**
 * @file wifi_manager.c
 * @brief WiFi Manager implementation for MiBuddy
 *
 * Code Flow:
 * 1. wifi_manager_init() is called from app_main()
 *    - Initializes ESP-IDF networking and WiFi stack
 *    - Registers event handlers for WIFI_EVENT and IP_EVENT
 *    - Starts connection with credentials from Kconfig
 *
 * 2. Event handler processes WiFi events:
 *    - WIFI_EVENT_STA_START: Triggers initial connection
 *    - WIFI_EVENT_STA_CONNECTED: Station connected to AP
 *    - WIFI_EVENT_STA_DISCONNECTED: Connection lost, triggers auto-retry
 *    - IP_EVENT_STA_GOT_IP: Full connection established, invokes callback
 *
 * 3. Status callback (provided by caller) is invoked:
 *    - On successful connection: (connected=true, rssi=signal_strength)
 *    - On disconnect: (connected=false, rssi=0)
 *
 * Thread Safety:
 * - Connection status is protected by atomic operations
 * - Callback invocation is from ESP event task context
 * - All public functions are thread-safe
 */

#include "wifi_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "sdkconfig.h"

static const char *TAG = "wifi_manager";

/* Event group bits for connection status */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* Module state */
static EventGroupHandle_t s_wifi_event_group = NULL;
static esp_netif_t *s_netif = NULL;
static esp_event_handler_instance_t s_instance_any_id = NULL;
static esp_event_handler_instance_t s_instance_got_ip = NULL;
static wifi_status_callback_t s_status_callback = NULL;
static volatile bool s_is_connected = false;
static volatile int s_current_rssi = 0;
static SemaphoreHandle_t s_scan_semaphore = NULL;
static volatile bool s_auto_reconnect = true;  /* Enable auto-reconnect by default */

/**
 * @brief Fetch current RSSI from connected AP
 * @return RSSI in dBm, or 0 if not connected
 */
static int fetch_current_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.rssi;
    }
    return 0;
}

/**
 * @brief WiFi event handler
 *
 * Handles all WiFi and IP events:
 * - Manages connection state
 * - Triggers auto-reconnect on disconnect
 * - Invokes status callback on state changes
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                /* WiFi stack started, initiate connection */
                ESP_LOGI(TAG, "WiFi started, connecting to %s...", CONFIG_MIBUDDY_WIFI_SSID);
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                /* Connected to AP (but no IP yet) */
                ESP_LOGI(TAG, "Connected to AP, waiting for IP...");
                break;

            case WIFI_EVENT_SCAN_DONE:
                /* WiFi scan completed - release semaphore */
                ESP_LOGI(TAG, "WiFi scan completed");
                if (s_scan_semaphore) {
                    xSemaphoreGive(s_scan_semaphore);
                }
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                /* Connection lost */
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
                ESP_LOGW(TAG, "Disconnected from AP (reason: %d)", event->reason);

                s_is_connected = false;
                s_current_rssi = 0;

                /* Notify callback of disconnect */
                if (s_status_callback) {
                    s_status_callback(false, 0);
                }

                /* Auto-retry only if enabled */
                if (s_auto_reconnect) {
                    ESP_LOGI(TAG, "Auto-reconnect enabled, retrying...");
                    vTaskDelay(pdMS_TO_TICKS(CONFIG_MIBUDDY_WIFI_RETRY_INTERVAL_MS));
                    esp_wifi_connect();
                } else {
                    ESP_LOGI(TAG, "Auto-reconnect disabled, staying disconnected");
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            /* Full connection established - we have an IP address */
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));

            s_is_connected = true;
            s_current_rssi = fetch_current_rssi();

            /* Log connected SSID */
            wifi_ap_record_t ap_info;
            if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
                ESP_LOGI(TAG, "Connected SSID: %s, RSSI: %d dBm", ap_info.ssid, s_current_rssi);
            }

            /* Set connected bit */
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

            /* Notify callback */
            if (s_status_callback) {
                s_status_callback(true, s_current_rssi);
            }
        }
    }
}

void wifi_manager_init(wifi_status_callback_t status_cb)
{
    ESP_LOGI(TAG, "Initializing WiFi manager...");
    ESP_LOGI(TAG, "Target SSID: %s", CONFIG_MIBUDDY_WIFI_SSID);

    s_status_callback = status_cb;

    /* Initialize TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create default event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Create default WiFi station interface */
    s_netif = esp_netif_create_default_wifi_sta();
    if (s_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return;
    }

    /* Initialize WiFi with default config */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Create event group for synchronization */
    s_wifi_event_group = xEventGroupCreate();

    /* Create semaphore for scan synchronization */
    s_scan_semaphore = xSemaphoreCreateBinary();

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &s_instance_any_id
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        &s_instance_got_ip
    ));

    /* Configure WiFi station with credentials from Kconfig */
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    /* Copy SSID and password (ensure null termination) */
    strncpy((char *)wifi_config.sta.ssid, CONFIG_MIBUDDY_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_MIBUDDY_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);

    /* Set mode and config */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    /* Start WiFi - this triggers WIFI_EVENT_STA_START */
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi manager initialized, connection in progress...");
}

bool wifi_manager_is_connected(void)
{
    return s_is_connected;
}

int wifi_manager_get_rssi(void)
{
    if (s_is_connected) {
        /* Fetch fresh RSSI value */
        s_current_rssi = fetch_current_rssi();
        return s_current_rssi;
    }
    return 0;
}

void wifi_manager_get_ip(char *ip_buf, size_t buf_len)
{
    if (ip_buf == NULL || buf_len == 0) {
        return;
    }

    if (s_is_connected && s_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_netif, &ip_info) == ESP_OK) {
            snprintf(ip_buf, buf_len, IPSTR, IP2STR(&ip_info.ip));
            return;
        }
    }

    snprintf(ip_buf, buf_len, "0.0.0.0");
}

void wifi_manager_get_gateway(char *gw_buf, size_t buf_len)
{
    if (gw_buf == NULL || buf_len == 0) {
        return;
    }

    if (s_is_connected && s_netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(s_netif, &ip_info) == ESP_OK) {
            snprintf(gw_buf, buf_len, IPSTR, IP2STR(&ip_info.gw));
            return;
        }
    }

    snprintf(gw_buf, buf_len, "0.0.0.0");
}

void wifi_manager_get_dns(char *dns_buf, size_t buf_len)
{
    if (dns_buf == NULL || buf_len == 0) {
        return;
    }

    if (s_is_connected && s_netif) {
        esp_netif_dns_info_t dns_info;
        if (esp_netif_get_dns_info(s_netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
            snprintf(dns_buf, buf_len, IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            return;
        }
    }

    snprintf(dns_buf, buf_len, "0.0.0.0");
}

void wifi_manager_get_ssid(char *ssid_buf, size_t buf_len)
{
    if (ssid_buf == NULL || buf_len == 0) {
        return;
    }

    if (s_is_connected) {
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            strncpy(ssid_buf, (char *)ap_info.ssid, buf_len - 1);
            ssid_buf[buf_len - 1] = '\0';
            return;
        }
    }

    ssid_buf[0] = '\0';
}

void wifi_manager_reconnect(void)
{
    ESP_LOGI(TAG, "Manual reconnect requested");
    s_auto_reconnect = true;
    esp_wifi_disconnect();
    esp_wifi_connect();
}

void wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "Manual disconnect requested");
    s_auto_reconnect = false;
    esp_wifi_disconnect();
}

void wifi_manager_connect(void)
{
    ESP_LOGI(TAG, "Manual connect requested");
    s_auto_reconnect = true;
    esp_wifi_connect();
}

bool wifi_manager_scan(wifi_ap_record_t *ap_info, uint16_t *ap_count, uint16_t max_aps, uint32_t timeout_ms)
{
    if (ap_info == NULL || ap_count == NULL || max_aps == 0) {
        return false;
    }

    ESP_LOGI(TAG, "Starting WiFi scan...");

    /* Clear any pending semaphore */
    xSemaphoreTake(s_scan_semaphore, 0);

    /* Start non-blocking scan */
    esp_err_t ret = esp_wifi_scan_start(NULL, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
        *ap_count = 0;
        return false;
    }

    /* Wait for scan to complete */
    if (xSemaphoreTake(s_scan_semaphore, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "Scan timeout after %lu ms", (unsigned long)timeout_ms);
        esp_wifi_scan_stop();
        *ap_count = 0;
        return false;
    }

    /* Get scan results */
    uint16_t found_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&found_count));

    uint16_t to_return = (found_count < max_aps) ? found_count : max_aps;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&to_return, ap_info));

    *ap_count = to_return;
    ESP_LOGI(TAG, "Scan complete: found %u networks, returning %u", found_count, to_return);

    return true;
}

void wifi_manager_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing WiFi manager...");

    /* Disconnect from AP */
    esp_wifi_disconnect();

    /* Stop WiFi */
    esp_err_t ret = esp_wifi_stop();
    if (ret == ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGW(TAG, "WiFi was not initialized");
        return;
    }

    /* Deinitialize WiFi driver */
    ESP_ERROR_CHECK(esp_wifi_deinit());

    /* Clean up network interface */
    if (s_netif) {
        esp_wifi_clear_default_wifi_driver_and_handlers(s_netif);
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }

    /* Unregister event handlers */
    if (s_instance_got_ip) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
        s_instance_got_ip = NULL;
    }

    if (s_instance_any_id) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
        s_instance_any_id = NULL;
    }

    /* Delete default event loop */
    esp_event_loop_delete_default();

    /* Clean up event group */
    if (s_wifi_event_group) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    /* Clean up scan semaphore */
    if (s_scan_semaphore) {
        vSemaphoreDelete(s_scan_semaphore);
        s_scan_semaphore = NULL;
    }

    s_is_connected = false;
    s_current_rssi = 0;
    s_status_callback = NULL;

    ESP_LOGI(TAG, "WiFi manager deinitialized");
}
