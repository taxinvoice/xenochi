/**
 * @file wifi_scan.c
 * @brief WiFi scanning UI for Settings app
 *
 * Provides a UI tile for scanning and displaying available WiFi networks.
 * Uses wifi_manager for all WiFi operations.
 *
 * Note: WiFi is initialized at boot by wifi_manager in main.cpp.
 * This module only handles scanning and display, not connection management.
 */

#include "wifi_scan.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lvgl_port.h"
#include "wifi_manager.h"



static lv_obj_t *list;
lv_obj_t *lable_wifi_ip;

#define LIST_BTN_LEN_MAX 10
lv_obj_t *list_btns[LIST_BTN_LEN_MAX];
uint16_t list_item_count = 0;

bool g_wifi_enable = true;

SemaphoreHandle_t wifi_scanf_semaphore;

static TaskHandle_t lvgl_wifi_task_handle = NULL;  // 任务句柄

static void btn_wifi_scan_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    // lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_CLICKED && g_wifi_enable)
    {
        for (int i = 0; i < list_item_count; i++)
        {
            //lv_obj_del(list_btns[i]);
        }
        list_item_count = 0;
        list_btns[list_item_count++] = lv_list_add_btn(list, NULL, "WiFi scanning underway!");

        xSemaphoreGive(wifi_scanf_semaphore);
        // app_wifi_scan((void*)wifi_infos, &list_item_count, 20);
        // for (int i = 0; i < list_item_count && i < LIST_BTN_LEN_MAX; i++)
        // {
        //     list_btns[i] = lv_list_add_btn(list, NULL, wifi_infos[i].name);
        //     label = lv_label_create(list_btns[i]);
        //     lv_label_set_text_fmt(label, "%d db", wifi_infos[i].rssi);
        //     list_item_count++;
        //     // lv_list_get_btn_index();
        // }
    }
}

static void sw_wifi_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (lv_obj_has_state(obj, LV_STATE_CHECKED))
        {
            g_wifi_enable = true;
            wifi_manager_reconnect();
        }
        else
        {
            g_wifi_enable = false;
            for (int i = 0; i < list_item_count; i++)
            {
                lv_obj_del(list_btns[i]);
            }
            list_item_count = 0;
            /* Note: We don't disconnect WiFi here anymore.
             * The wifi_manager handles the connection lifecycle.
             */
        }
    }
}

/**
 * @brief Background task for WiFi scanning and IP display
 *
 * Waits for scan requests via semaphore, performs scan using wifi_manager,
 * and updates the UI with results. Also periodically updates the IP display.
 */
static void lvgl_wifi_task(void *arg)
{
    char str[50] = {0};
    char str_wifi_ip[32] = {0};
    lv_obj_t *label;
    wifi_ap_record_t ap_info[LIST_BTN_LEN_MAX];
    uint16_t scan_number = 0;

    /* Note: WiFi is already initialized by wifi_manager at boot */

    while (1)
    {
        if (xSemaphoreTake(wifi_scanf_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            printf("wifi_scanf!!\r\n");
            memset(ap_info, 0, sizeof(ap_info));

            /* Use wifi_manager for scanning */
            if (wifi_manager_scan(ap_info, &scan_number, LIST_BTN_LEN_MAX, 5000))
            {
                if (lvgl_port_lock(0))
                {
                    for (int i = 0; i < scan_number && i < LIST_BTN_LEN_MAX; i++)
                    {
                        list_btns[i] = lv_list_add_btn(list, NULL, (char *)ap_info[i].ssid);
                        label = lv_label_create(list_btns[i]);
                        lv_label_set_text_fmt(label, "%d db", ap_info[i].rssi);
                    }
                    list_item_count = scan_number;
                    lvgl_port_unlock();
                }
            }
        }

        /* Update IP display using wifi_manager */
        wifi_manager_get_ip(str_wifi_ip, sizeof(str_wifi_ip));
        sprintf(str, "IP: %s", str_wifi_ip);

        if (lvgl_port_lock(0))
        {
            lv_label_set_text(lable_wifi_ip, str);
            lvgl_port_unlock();
        }
    }
}

static lv_obj_t *btn;
void wifi_tile_init(lv_obj_t *parent)
{
    wifi_scanf_semaphore = xSemaphoreCreateBinary();

    lv_obj_t *lable;

    //printf("wifi_tile_init 1  !!\r\n");

    lable_wifi_ip = lv_label_create(parent);
    lv_label_set_text(lable_wifi_ip, "IP: 0.0.0.0");
    lv_obj_align(lable_wifi_ip, LV_ALIGN_TOP_MID, 0, 0);

    btn = lv_btn_create(parent);
    lable = lv_label_create(btn);
    lv_label_set_text(lable, "Scan");
    lv_obj_center(lable);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(btn, btn_wifi_scan_event_handler, LV_EVENT_CLICKED, NULL);

    //printf("wifi_tile_init 2  !!\r\n");

    // lv_obj_t *sw = lv_switch_create(parent);
    // lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    // lv_obj_add_event_cb(sw, sw_wifi_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
    // lv_obj_add_state(sw, LV_STATE_CHECKED);

    list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(95), lv_pct(85));
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    //printf("wifi_tile_init 3  !!\r\n");
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
    lv_obj_remove_event_cb(btn, btn_wifi_scan_event_handler);
    btn = NULL;

    if (lvgl_wifi_task_handle != NULL) {
        vTaskDelete(lvgl_wifi_task_handle);
        lvgl_wifi_task_handle = NULL;
    }

    /* Note: Do NOT call wifi_manager_deinit() here.
     * WiFi should stay connected even when Settings app is closed.
     * wifi_manager manages the WiFi lifecycle globally.
     */
}