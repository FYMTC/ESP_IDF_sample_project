#if 0
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#define FILENAME "/sdcard/mouse_device.txt"
static const char *TAG = "BLE_MOUSE_TASK";

static ble_addr_t saved_address = {0};
static bool doConnect = false;
static bool isConnected = false;
static uint16_t conn_handle;

int8_t mouseX;
int8_t mouseY;
bool mouseLeftButton;
static bool mouseRightButton = false;
uint8_t notifyCallback_statue;
bool mouse_indev_statue;


SemaphoreHandle_t sdCardMutex;

void startScan(void *parameter);
bool connectTaskFinished = false;

static void ble_app_scan(void);
static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_app_on_sync(void);

void notifyCallback0(uint8_t *pData, size_t length)
{
    // Parse mouse data here
    mouseX = pData[1]; // X movement
    mouseY = pData[3]; // Y movement
    mouseLeftButton = pData[0] & 0x01;
    mouseRightButton = pData[0] & 0x02;
    notifyCallback_statue++;
}

void saveDeviceAddress(ble_addr_t address)
{
    if (xSemaphoreTake(sdCardMutex, portMAX_DELAY) == pdTRUE) {
        FILE *file = fopen(FILENAME, "w");
        if (!file)
        {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }
        fprintf(file, "%02X:%02X:%02X:%02X:%02X:%02X\n", 
                address.val[0], address.val[1], address.val[2], 
                address.val[3], address.val[4], address.val[5]);
        fclose(file);
        xSemaphoreGive(sdCardMutex); // Release the mutex
    }
    ESP_LOGI(TAG, "Device address saved to SD card");
}

ble_addr_t readDeviceAddress()
{
    ble_addr_t address = {0};
    if (xSemaphoreTake(sdCardMutex, portMAX_DELAY) == pdTRUE) {
        FILE *file = fopen(FILENAME, "r");
        if (!file)
        {
            ESP_LOGE(TAG, "Failed to open file for reading");
            return address;
        }

        fscanf(file, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", 
               &address.val[0], &address.val[1], &address.val[2], 
               &address.val[3], &address.val[4], &address.val[5]);
        fclose(file);
        xSemaphoreGive(sdCardMutex); // Release the mutex
    }
    return address;
}

static void ble_app_scan(void)
{
    struct ble_gap_disc_params disc_params = {
        .itvl = 100,                // Scan interval
        .window = 50,               // Scan window
        .filter_policy = 0,         // Filter policy
        .limited = 0,               // Limited discovery
        .passive = 0,               // Passive scan
        .filter_duplicates = 1      // Filter duplicates
    };

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start discovery; rc=%d", rc);
    }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
        case BLE_GAP_EVENT_DISC:
            ESP_LOGI(TAG, "Discovered device: %s", event->disc.addr.val);
            if (memcmp(event->disc.addr.val, saved_address.val, 6) == 0) {
                ESP_LOGI(TAG, "Found device with saved address.");
                ble_gap_disc_cancel();
                doConnect = true;
            }
            break;

        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected to device.");
                conn_handle = event->connect.conn_handle;
                isConnected = true;
            } else {
                ESP_LOGE(TAG, "Failed to connect to device.");
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected from device.");
            isConnected = false;
            doConnect = true;
            break;

        default:
            break;
    }
    return 0;
}

static void ble_app_on_sync(void)
{
    int rc;
    ble_addr_t addr;

    rc = ble_hs_id_infer_auto(0, (uint8_t *)&addr);
    assert(rc == 0);

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    ble_app_scan();
}

void ble_host_task(void *param)
{
    nimble_port_run();
}

void bt_start()
{
    esp_err_t ret;

    // Initialize NimBLE
    nimble_port_init();
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(ble_host_task);

    // Initialize SD card
    sdCardMutex = xSemaphoreCreateMutex();

    // Read saved address
    saved_address = readDeviceAddress();
    if (memcmp(saved_address.val, "\0\0\0\0\0\0", 6) != 0) {
        doConnect = true;
    } else {
        ble_app_scan();
    }

    // Main loop
    while (1) {
        if (!isConnected && doConnect) {
            struct ble_gap_conn_params conn_params = {0};
            conn_params.scan_itvl = 16;
            conn_params.scan_window = 16;
            conn_params.itvl_min = 24;
            conn_params.itvl_max = 40;
            conn_params.latency = 0;
            conn_params.supervision_timeout = 256;
            conn_params.min_ce_len = 16;
            conn_params.max_ce_len = 32;

            int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &saved_address, 30000, &conn_params, ble_gap_event, NULL);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to connect to device; rc=%d", rc);
            }
            doConnect = false;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
#endif