/**
 * @file wifi_scan.c
 * @brief WiFi scanning and status UI for Settings app
 *
 * Provides a tabbed UI for:
 * - Status tab: IP, Gateway, DNS, SSID with Connect/Disconnect buttons
 * - Scan tab: Scan and display available WiFi networks
 *
 * Uses wifi_manager for all WiFi operations.
 *
 * Note: WiFi is initialized at boot by wifi_manager in main.cpp.
 * This module only handles UI display and user interactions.
 */

#include "wifi_scan.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "wifi_manager.h"

/* Scan tab elements */
static lv_obj_t *list;
#define LIST_BTN_LEN_MAX 10
static lv_obj_t *list_btns[LIST_BTN_LEN_MAX];
static uint16_t list_item_count = 0;
static lv_obj_t *btn_scan = NULL;

/* Status tab elements */
static lv_obj_t *label_ip = NULL;
static lv_obj_t *label_gateway = NULL;
static lv_obj_t *label_dns = NULL;
static lv_obj_t *label_ssid = NULL;
static lv_obj_t *label_status = NULL;
static lv_obj_t *btn_connect = NULL;
static lv_obj_t *btn_disconnect = NULL;

/* Shared state */
static bool g_wifi_enable = true;
static SemaphoreHandle_t wifi_scanf_semaphore = NULL;
static TaskHandle_t lvgl_wifi_task_handle = NULL;
static bool g_action_pending = false;  /* Track if connect/disconnect action is in progress */
static bool g_last_connected_state = false;  /* Track last known connection state */

/**
 * @brief Scan button click event handler
 */
static void btn_wifi_scan_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED && g_wifi_enable)
    {
        /* Disable scan button to prevent multiple clicks */
        lv_obj_add_state(btn_scan, LV_STATE_DISABLED);

        /* Clear existing list items */
        lv_obj_clean(list);
        list_item_count = 0;
        lv_list_add_btn(list, NULL, "Scanning...");

        xSemaphoreGive(wifi_scanf_semaphore);
    }
}

/**
 * @brief Connect button click event handler
 */
static void btn_connect_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        /* Disable both buttons to prevent multiple clicks */
        lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
        lv_obj_add_state(btn_disconnect, LV_STATE_DISABLED);
        g_action_pending = true;

        wifi_manager_connect();
        g_wifi_enable = true;
    }
}

/**
 * @brief Disconnect button click event handler
 */
static void btn_disconnect_event_handler(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        /* Disable both buttons to prevent multiple clicks */
        lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
        lv_obj_add_state(btn_disconnect, LV_STATE_DISABLED);
        g_action_pending = true;

        wifi_manager_disconnect();
        g_wifi_enable = false;
    }
}

/**
 * @brief Update status tab labels with current network info
 */
static void update_status_labels(void)
{
    char buf[32];
    bool is_connected = wifi_manager_is_connected();

    /* Check if connection state changed - re-enable buttons */
    if (g_action_pending && (is_connected != g_last_connected_state)) {
        g_action_pending = false;
        if (btn_connect != NULL) {
            lv_obj_clear_state(btn_connect, LV_STATE_DISABLED);
        }
        if (btn_disconnect != NULL) {
            lv_obj_clear_state(btn_disconnect, LV_STATE_DISABLED);
        }
    }
    g_last_connected_state = is_connected;

    /* Update connection status */
    if (is_connected) {
        int rssi = wifi_manager_get_rssi();
        snprintf(buf, sizeof(buf), "Connected (%d dBm)", rssi);
        lv_label_set_text(label_status, buf);
    } else {
        lv_label_set_text(label_status, "Disconnected");
    }

    /* Update SSID */
    wifi_manager_get_ssid(buf, sizeof(buf));
    if (buf[0] != '\0') {
        char ssid_text[48];
        snprintf(ssid_text, sizeof(ssid_text), "SSID: %s", buf);
        lv_label_set_text(label_ssid, ssid_text);
    } else {
        lv_label_set_text(label_ssid, "SSID: --");
    }

    /* Update IP */
    wifi_manager_get_ip(buf, sizeof(buf));
    char ip_text[48];
    snprintf(ip_text, sizeof(ip_text), "IP: %s", buf);
    lv_label_set_text(label_ip, ip_text);

    /* Update Gateway */
    wifi_manager_get_gateway(buf, sizeof(buf));
    char gw_text[48];
    snprintf(gw_text, sizeof(gw_text), "Gateway: %s", buf);
    lv_label_set_text(label_gateway, gw_text);

    /* Update DNS */
    wifi_manager_get_dns(buf, sizeof(buf));
    char dns_text[48];
    snprintf(dns_text, sizeof(dns_text), "DNS: %s", buf);
    lv_label_set_text(label_dns, dns_text);
}

/**
 * @brief Background task for WiFi scanning and status updates
 *
 * Waits for scan requests via semaphore, performs scan using wifi_manager,
 * and updates the UI with results. Also periodically updates the status display.
 */
static void lvgl_wifi_task(void *arg)
{
    lv_obj_t *label;
    wifi_ap_record_t ap_info[LIST_BTN_LEN_MAX];
    uint16_t scan_number = 0;

    while (1)
    {
        /* Check for scan request */
        if (xSemaphoreTake(wifi_scanf_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            printf("WiFi scan started\r\n");
            memset(ap_info, 0, sizeof(ap_info));

            /* Use wifi_manager for scanning */
            if (wifi_manager_scan(ap_info, &scan_number, LIST_BTN_LEN_MAX, 5000))
            {
                if (lvgl_port_lock(0))
                {
                    /* Clear list and add scan results */
                    lv_obj_clean(list);
                    for (int i = 0; i < scan_number && i < LIST_BTN_LEN_MAX; i++)
                    {
                        list_btns[i] = lv_list_add_btn(list, NULL, (char *)ap_info[i].ssid);
                        label = lv_label_create(list_btns[i]);
                        lv_label_set_text_fmt(label, "%d dB", ap_info[i].rssi);
                    }
                    list_item_count = scan_number;

                    if (scan_number == 0) {
                        lv_list_add_btn(list, NULL, "No networks found");
                    }

                    /* Re-enable scan button */
                    if (btn_scan != NULL) {
                        lv_obj_clear_state(btn_scan, LV_STATE_DISABLED);
                    }

                    lvgl_port_unlock();
                }
            }
            else
            {
                if (lvgl_port_lock(0))
                {
                    lv_obj_clean(list);
                    lv_list_add_btn(list, NULL, "Scan failed");

                    /* Re-enable scan button */
                    if (btn_scan != NULL) {
                        lv_obj_clear_state(btn_scan, LV_STATE_DISABLED);
                    }

                    lvgl_port_unlock();
                }
            }
        }

        /* Update status labels periodically */
        if (lvgl_port_lock(0))
        {
            update_status_labels();
            lvgl_port_unlock();
        }
    }
}

/**
 * @brief Create the Status tab content
 */
static void create_status_tab(lv_obj_t *parent)
{
    /* Configure parent with column flex layout */
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(parent, 5, 0);
    lv_obj_set_style_pad_row(parent, 4, 0);

    /* Status label */
    label_status = lv_label_create(parent);
    lv_label_set_text(label_status, "Status: --");

    /* SSID label */
    label_ssid = lv_label_create(parent);
    lv_label_set_text(label_ssid, "SSID: --");

    /* IP label */
    label_ip = lv_label_create(parent);
    lv_label_set_text(label_ip, "IP: 0.0.0.0");

    /* Gateway label */
    label_gateway = lv_label_create(parent);
    lv_label_set_text(label_gateway, "Gateway: 0.0.0.0");

    /* DNS label */
    label_dns = lv_label_create(parent);
    lv_label_set_text(label_dns, "DNS: 0.0.0.0");

    /* Spacer */
    lv_obj_t *spacer = lv_obj_create(parent);
    lv_obj_set_size(spacer, lv_pct(100), 10);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);

    /* Button container */
    lv_obj_t *btn_cont = lv_obj_create(parent);
    lv_obj_set_size(btn_cont, lv_pct(100), 40);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);

    /* Connect button */
    btn_connect = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_connect, 80, 30);
    lv_obj_add_event_cb(btn_connect, btn_connect_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_connect = lv_label_create(btn_connect);
    lv_label_set_text(lbl_connect, "Connect");
    lv_obj_center(lbl_connect);

    /* Disconnect button */
    btn_disconnect = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_disconnect, 80, 30);
    lv_obj_add_event_cb(btn_disconnect, btn_disconnect_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_disconnect = lv_label_create(btn_disconnect);
    lv_label_set_text(lbl_disconnect, "Disconnect");
    lv_obj_center(lbl_disconnect);

    /* Initial update */
    update_status_labels();
}

/**
 * @brief Create the Scan tab content
 */
static void create_scan_tab(lv_obj_t *parent)
{
    /* Configure parent with column flex layout */
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 5, 0);

    /* Scan button */
    btn_scan = lv_btn_create(parent);
    lv_obj_set_size(btn_scan, 100, 30);
    lv_obj_add_event_cb(btn_scan, btn_wifi_scan_event_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, "Scan");
    lv_obj_center(lbl_scan);

    /* Network list */
    list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(85));
    lv_obj_set_flex_grow(list, 1);

    /* Add placeholder text */
    lv_list_add_btn(list, NULL, "Press Scan to search");
}

/**
 * @brief Initialize the WiFi tile with tabbed interface
 *
 * Creates a tabview with:
 * - Status tab: Network info and connect/disconnect buttons
 * - Scan tab: WiFi network scanner
 */
void wifi_tile_init(lv_obj_t *parent)
{
    /* Create semaphore for scan synchronization */
    wifi_scanf_semaphore = xSemaphoreCreateBinary();

    /* Create tabview */
    lv_obj_t *tabview = lv_tabview_create(parent);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 30);
    lv_obj_set_size(tabview, lv_pct(100), lv_pct(100));

    /* Create tabs */
    lv_obj_t *tab_status = lv_tabview_add_tab(tabview, "Status");
    lv_obj_t *tab_scan = lv_tabview_add_tab(tabview, "Scan");

    /* Populate tabs */
    create_status_tab(tab_status);
    create_scan_tab(tab_scan);

    /* Start background task for scanning and status updates */
    xTaskCreate(lvgl_wifi_task, "lvgl_wifi_task", 1024 * 10, NULL, 1, &lvgl_wifi_task_handle);
}

/**
 * @brief Clean up WiFi scan task and UI resources
 *
 * Called when Settings app WiFi tile is closed.
 * Note: WiFi connection is NOT terminated - it stays connected.
 */
void delete_lv_wifi_scan_task(void)
{
    /* Remove event callbacks */
    if (btn_scan != NULL) {
        lv_obj_remove_event_cb(btn_scan, btn_wifi_scan_event_handler);
        btn_scan = NULL;
    }
    if (btn_connect != NULL) {
        lv_obj_remove_event_cb(btn_connect, btn_connect_event_handler);
        btn_connect = NULL;
    }
    if (btn_disconnect != NULL) {
        lv_obj_remove_event_cb(btn_disconnect, btn_disconnect_event_handler);
        btn_disconnect = NULL;
    }

    /* Delete background task */
    if (lvgl_wifi_task_handle != NULL) {
        vTaskDelete(lvgl_wifi_task_handle);
        lvgl_wifi_task_handle = NULL;
    }

    /* Clean up semaphore */
    if (wifi_scanf_semaphore != NULL) {
        vSemaphoreDelete(wifi_scanf_semaphore);
        wifi_scanf_semaphore = NULL;
    }

    /* Reset static pointers and state */
    list = NULL;
    label_ip = NULL;
    label_gateway = NULL;
    label_dns = NULL;
    label_ssid = NULL;
    label_status = NULL;
    list_item_count = 0;
    g_action_pending = false;
    g_last_connected_state = false;

    /* Note: Do NOT call wifi_manager_deinit() here.
     * WiFi should stay connected even when Settings app is closed.
     * wifi_manager manages the WiFi lifecycle globally.
     */
}
