/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "bt.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "esp_hidh.h"
#include "esp_hid_gap.h"

static const char *TAG = "ESP_HIDH_DEMO";
extern mouse_t bt_mouse_indev;

#define NVS_NAMESPACE "bt_hid"
#define NVS_KEY_DEVICE_ADDR "dev_addr"
#define NVS_KEY_DEVICE_TRANSPORT "dev_transport"
#define NVS_KEY_DEVICE_ADDR_TYPE "dev_addr_type"

bool ble_scan_flag = false; // 是否扫描标志位

// 存储设备地址到NVS
static void store_device_info_to_nvs(const uint8_t *bda, esp_hid_transport_t transport, uint8_t addr_type)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // 打开NVS命名空间
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS namespace: %s", esp_err_to_name(err));
        return;
    }

    // 存储设备地址
    err = nvs_set_blob(nvs_handle, NVS_KEY_DEVICE_ADDR, bda, ESP_BD_ADDR_LEN);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error storing device address: %s", esp_err_to_name(err));
    }

    // 存储传输类型
    err = nvs_set_u8(nvs_handle, NVS_KEY_DEVICE_TRANSPORT, (uint8_t)transport);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error storing device transport: %s", esp_err_to_name(err));
    }

    // 存储地址类型（仅BLE需要）
    err = nvs_set_u8(nvs_handle, NVS_KEY_DEVICE_ADDR_TYPE, addr_type);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error storing device address type: %s", esp_err_to_name(err));
    }

    // 提交更改
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error committing NVS changes: %s", esp_err_to_name(err));
    }

    // 关闭NVS
    nvs_close(nvs_handle);
}

// 从NVS读取设备地址
static bool load_device_info_from_nvs(uint8_t *bda, esp_hid_transport_t *transport, uint8_t *addr_type)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // 打开NVS命名空间
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS namespace: %s", esp_err_to_name(err));
        return false;
    }

    // 读取设备地址
    size_t length = ESP_BD_ADDR_LEN;
    err = nvs_get_blob(nvs_handle, NVS_KEY_DEVICE_ADDR, bda, &length);
    if (err != ESP_OK || length != ESP_BD_ADDR_LEN)
    {
        ESP_LOGE(TAG, "Error reading device address: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // 读取传输类型
    uint8_t transport_u8;
    err = nvs_get_u8(nvs_handle, NVS_KEY_DEVICE_TRANSPORT, &transport_u8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading device transport: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    *transport = (esp_hid_transport_t)transport_u8;

    // 读取地址类型（仅BLE需要）
    err = nvs_get_u8(nvs_handle, NVS_KEY_DEVICE_ADDR_TYPE, addr_type);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading device address type: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    // 关闭NVS
    nvs_close(nvs_handle);
    return true;
}

// 尝试连接NVS中存储的设备
static bool try_connect_stored_device()
{
    uint8_t bda[ESP_BD_ADDR_LEN];
    esp_hid_transport_t transport;
    uint8_t addr_type;

    // 从NVS中读取设备信息
    if (!load_device_info_from_nvs(bda, &transport, &addr_type))
    {
        ESP_LOGI(TAG, "No stored device found in NVS.");
        ble_scan_flag = true;
        return false;
    }

    ESP_LOGI(TAG, "Attempting to connect to stored device: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(bda));

    // 尝试连接设备
    esp_hidh_dev_t *dev = esp_hidh_dev_open(bda, transport, addr_type);
    if (dev == NULL)
    {
        ESP_LOGE(TAG, "Failed to create HID device object.");
        ble_scan_flag = true;
        return false;
    }

    ESP_LOGI(TAG, "HID device object created, waiting for connection...");
    return true;
}

void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;

    switch (event)
    {
    case ESP_HIDH_OPEN_EVENT:
    {
        if (param->open.status == ESP_OK)
        {
            const uint8_t *bda = esp_hidh_dev_bda_get(param->open.dev);
            ESP_LOGI(TAG, ESP_BD_ADDR_STR " OPEN: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->open.dev));
            esp_hidh_dev_dump(param->open.dev, stdout);

            ble_scan_flag = false; // 连接成功后停止扫描
        }
        else
        {
            ESP_LOGE(TAG, " OPEN failed!");
        }
        break;
    }
    case ESP_HIDH_BATTERY_EVENT:
    {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->battery.dev);
        // ESP_LOGI(TAG, ESP_BD_ADDR_STR " BATTERY: %d%%", ESP_BD_ADDR_HEX(bda), param->battery.level);
        break;
    }
    case ESP_HIDH_INPUT_EVENT:
    {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->input.dev);
        // ESP_LOGI(TAG, ESP_BD_ADDR_STR " INPUT: %8s, MAP: %2u, ID: %3u, Len: %d, Data:", ESP_BD_ADDR_HEX(bda), esp_hid_usage_str(param->input.usage), param->input.map_index, param->input.report_id, param->input.length);
        // ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);

        // 显示鼠标移动信息
        if (param->input.usage == ESP_HID_USAGE_MOUSE)
        {

            // 假设鼠标报告长度为 7 字节，格式为 [按钮状态, X 移动, Y 移动, 滚轮]
            if (param->input.length >= 4)
            {
                bt_mouse_indev.button_state = param->input.data[0]; // 按钮状态
                bt_mouse_indev.left_button_pressed = param->input.data[0] == 1 ? true : false;
                bt_mouse_indev.right_button_pressed = param->input.data[0] == 2 ? true : false;
                bt_mouse_indev.x_movement = param->input.data[1];            // X 轴移动
                bt_mouse_indev.x_movement_direction = param->input.data[2];  // X 轴移动状态
                bt_mouse_indev.y_movement = param->input.data[3];            // Y 轴移动
                bt_mouse_indev.y_movement_direction = -param->input.data[4]; // Y 轴移动状态
                bt_mouse_indev.wheel_movement = param->input.data[5];        // 滚轮移动
                bt_mouse_indev.data_frame++;

                // ESP_LOGI(TAG, "Button=0x%02X, X=%d, Y=%d, Wheel=%d", bt_mouse_indev.button_state, bt_mouse_indev.x_movement,bt_mouse_indev.y_movement, bt_mouse_indev.wheel_movement);
                //

                // switch (bt_mouse_indev.button_state)
                // {
                // case 1:
                //     {ESP_LOGI(TAG, "Left Button Pressed");
                //     break;}
                // case 2:
                //     {ESP_LOGI(TAG, "Right Button Pressed");
                //     break;}
                // case 8:
                //     {ESP_LOGI(TAG, "left down Button Pressed");
                //     break;}
                // case 16:
                //     {ESP_LOGI(TAG, "left up Button Pressed");
                //     break;}

                // default:
                //     break;
                // }
            }
        }

        break;
    }
    case ESP_HIDH_FEATURE_EVENT:
    {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->feature.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " FEATURE: %8s, MAP: %2u, ID: %3u, Len: %d", ESP_BD_ADDR_HEX(bda),
                 esp_hid_usage_str(param->feature.usage), param->feature.map_index, param->feature.report_id,
                 param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);

        ble_scan_flag = true;

        break;
    }
    case ESP_HIDH_CLOSE_EVENT:
    {
        const uint8_t *bda = esp_hidh_dev_bda_get(param->close.dev);
        ESP_LOGI(TAG, ESP_BD_ADDR_STR " CLOSE: %s", ESP_BD_ADDR_HEX(bda), esp_hidh_dev_name_get(param->close.dev));
        ESP_LOGI(TAG, "Device disconnected. Restarting scan...");
        ble_scan_flag = true;
        break;
    }
    default:
        ESP_LOGI(TAG, "EVENT: %d", event);
        break;
    }
}

#define SCAN_DURATION_SECONDS 2

void hid_demo_task(void *pvParameters)
{
    // 尝试连接NVS中存储的设备
    ble_scan_flag = try_connect_stored_device() ? false : true;
    while (1)
    {

        if (ble_scan_flag)
        {
            size_t results_len = 0;                // 存储扫描到的设备数量
            esp_hid_scan_result_t *results = NULL; // 存储扫描到的设备列表
            ESP_LOGI(TAG, "SCAN...");              // 打印日志信息，表示开始扫描
            // 开始扫描 HID 设备，扫描时间为 SCAN_DURATION_SECONDS 秒
            esp_hid_scan(SCAN_DURATION_SECONDS, &results_len, &results);
            ESP_LOGI(TAG, "SCAN: %u results", results_len); // 打印扫描到的设备数量

            if (results_len) // 如果扫描到了设备
            {
                esp_hid_scan_result_t *r = results; // 指向扫描结果列表的指针
                esp_hid_scan_result_t *cr = NULL;   // 用于存储最后一个扫描到的设备
                while (r)                           // 遍历扫描结果列表
                {
                    // 打印设备信息，包括传输类型、地址、RSSI、用途等
                    //printf("  %s: " ESP_BD_ADDR_STR ", ", (r->transport == ESP_HID_TRANSPORT_BLE) ? "BLE" : "BT ", ESP_BD_ADDR_HEX(r->bda));
                    //printf("RSSI: %d, ", r->rssi);
                    //printf("USAGE: %s, ", esp_hid_usage_str(r->usage));
#if CONFIG_BT_BLE_ENABLED                                      // 如果启用了 BLE
                    if (r->transport == ESP_HID_TRANSPORT_BLE) // 如果设备是 BLE 设备
                    {
                        cr = r;                                                           // 将当前设备设为最后一个设备
                        printf("APPEARANCE: 0x%04x, ", r->ble.appearance);                // 打印设备外观
                        printf("ADDR_TYPE: '%s', ", ble_addr_type_str(r->ble.addr_type)); // 打印地址类型
                    }
#endif                                                        /* CONFIG_BT_BLE_ENABLED */
#if CONFIG_BT_HID_HOST_ENABLED                                // 如果启用了 HID 主机
                    if (r->transport == ESP_HID_TRANSPORT_BT) // 如果设备是经典蓝牙设备
                    {
                        cr = r;                                                     // 将当前设备设为最后一个设备
                        printf("COD: %s[", esp_hid_cod_major_str(r->bt.cod.major)); // 打印设备类别
                        esp_hid_cod_minor_print(r->bt.cod.minor, stdout);           // 打印设备子类别
                        printf("] srv 0x%03x, ", r->bt.cod.service);                // 打印服务
                        print_uuid(&r->bt.uuid);                                    // 打印 UUID
                        printf(", ");
                    }
#endif                                                           /* CONFIG_BT_HID_HOST_ENABLED */
                    printf("NAME: %s ", r->name ? r->name : ""); // 打印设备名称
                    printf("\n");
                    r = r->next; // 移动到下一个设备
                }
                if (cr)
                {
                    // 打开最后一个扫描到的设备
                    ESP_LOGI(TAG, "Connecting to device: " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(cr->bda));
                    esp_hidh_dev_open(cr->bda, cr->transport, cr->ble.addr_type);
                    // 存储设备信息到NVS
                    store_device_info_to_nvs(cr->bda, cr->transport, cr->ble.addr_type);
                }
                // 释放扫描结果
                esp_hid_scan_results_free(results);
            }
            else
            {
                ESP_LOGI(TAG, "No devices found. Retrying scan...");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 秒后重新扫描
    }
}
TaskHandle_t bt_hid_taskhandle;
void bt_host_start(void)
{

#if HID_HOST_MODE == HIDH_IDLE_MODE
    ESP_LOGE(TAG, "Please turn on BT HID host or BLE!");
    return;
#endif
    // esp_err_t ret;
    // ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    // {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_HOST_MODE);
    ESP_ERROR_CHECK(esp_hid_gap_init(HID_HOST_MODE));
#if CONFIG_BT_BLE_ENABLED
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_hidh_gattc_event_handler));
#endif /* CONFIG_BT_BLE_ENABLED */
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(esp_hidh_init(&config));
    xTaskCreate(&hid_demo_task, "bt_hid_task", 3 * 1024, NULL, 1, &bt_hid_taskhandle);
}
