/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "msc_host.h"
#include "msc_host_vfs.h"
#include "ffconf.h"
#include "esp_vfs.h"
#include "errno.h"
#include "hal/usb_hal.h"
#include "driver/gpio.h"
#include <esp_vfs_fat.h>

// 定义USB断开连接的GPIO引脚
#define USB_DISCONNECT_PIN  GPIO_NUM_10

// 定义USB主机库可以卸载的条件
#define READY_TO_UNINSTALL (HOST_NO_CLIENT | HOST_ALL_FREE)

// 定义应用事件枚举
typedef enum {
    HOST_NO_CLIENT = 0x1,        // 没有客户端
    HOST_ALL_FREE = 0x2,         // 所有设备都已释放
    DEVICE_CONNECTED = 0x4,      // 设备已连接
    DEVICE_DISCONNECTED = 0x8,   // 设备已断开
    DEVICE_ADDRESS_MASK = 0xFF0, // 设备地址掩码
} app_event_t;

// 日志标签
static const char *TAG = "example";
// 事件组句柄
static EventGroupHandle_t usb_flags;

// MSC事件回调函数
static void msc_event_cb(const msc_host_event_t *event, void *arg)
{
    if (event->event == MSC_DEVICE_CONNECTED) {
        ESP_LOGI(TAG, "MSC device connected");
        // 获取的USB设备地址放在应用事件之后
        xEventGroupSetBits(usb_flags, DEVICE_CONNECTED | (event->device.address << 4));
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        xEventGroupSetBits(usb_flags, DEVICE_DISCONNECTED);
        ESP_LOGI(TAG, "MSC device disconnected");
    }
}

// 打印设备信息
static void print_device_info(msc_host_device_info_t *info)
{
    const size_t megabyte = 1024 * 1024;
    uint64_t capacity = ((uint64_t)info->sector_size * info->sector_count) / megabyte;

    printf("Device info:\n");
    printf("\t Capacity: %llu MB\n", capacity);
    printf("\t Sector size: %"PRIu32"\n", info->sector_size);
    printf("\t Sector count: %"PRIu32"\n", info->sector_count);
    printf("\t PID: 0x%4X \n", info->idProduct);
    printf("\t VID: 0x%4X \n", info->idVendor);
    wprintf(L"\t iProduct: %S \n", info->iProduct);
    wprintf(L"\t iManufacturer: %S \n", info->iManufacturer);
    wprintf(L"\t iSerialNumber: %S \n", info->iSerialNumber);
}

// 检查文件是否存在
static bool file_exists(const char *file_path)
{
    struct stat buffer;
    return stat(file_path, &buffer) == 0;
}

// 文件操作
static void file_operations(void)
{
    const char *directory = "/usb/esp";
    const char *file_path = "/usb/esp/test.txt";

    struct stat s = {0};
    bool directory_exists = stat(directory, &s) == 0;
    if (!directory_exists) {
        if (mkdir(directory, 0775) != 0) {
            ESP_LOGE(TAG, "mkdir failed with errno: %s\n", strerror(errno));
        }
    }

    if (!file_exists(file_path)) {
        ESP_LOGI(TAG, "Creating file");
        FILE *f = fopen(file_path, "w");
        if (f == NULL) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }
        fprintf(f, "Hello World!\n");
        fclose(f);
    }

    FILE *f;
    ESP_LOGI(TAG, "Reading file");
    f = fopen(file_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return;
    }
    char line[64];
    fgets(line, sizeof(line), f);
    fclose(f);
    // 去除换行符
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);
}

// 处理USB主机库的常见事件
static void handle_usb_events(void *args)
{
    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        // 在所有客户端注销后释放设备
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
            xEventGroupSetBits(usb_flags, HOST_NO_CLIENT);
        }
        // 给ready_to_uninstall_usb信号量，表示可以卸载USB主机库，并终止此任务
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            xEventGroupSetBits(usb_flags, HOST_ALL_FREE);
        }
    }

    vTaskDelete(NULL);
}

// 等待MSC设备连接
static uint8_t wait_for_msc_device(void)
{
    EventBits_t event;

    ESP_LOGI(TAG, "Waiting for USB stick to be connected");
    event = xEventGroupWaitBits(usb_flags, DEVICE_CONNECTED | DEVICE_ADDRESS_MASK,
                                pdTRUE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "connection...");
    // 从事件组位中提取USB设备地址
    return (event & DEVICE_ADDRESS_MASK) >> 4;
}

// 等待特定事件
static bool wait_for_event(EventBits_t event, TickType_t timeout)
{
    return xEventGroupWaitBits(usb_flags, event, pdTRUE, pdTRUE, timeout) & event;
}

// 主应用程序入口
void msc_host(void)
{
    msc_host_device_handle_t msc_device;
    msc_host_vfs_handle_t vfs_handle;
    msc_host_device_info_t info;
    BaseType_t task_created;

    // 配置USB断开连接的GPIO引脚为输入模式
    const gpio_config_t input_pin = {
        .pin_bit_mask = BIT64(USB_DISCONNECT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK( gpio_config(&input_pin) );

    // 创建事件组
    usb_flags = xEventGroupCreate();
    assert(usb_flags);

    // 配置并安装USB主机库
    const usb_host_config_t host_config = { .intr_flags = ESP_INTR_FLAG_LEVEL1 };
    ESP_ERROR_CHECK( usb_host_install(&host_config) );
    task_created = xTaskCreate(handle_usb_events, "usb_events", 2048, NULL, 2, NULL);
    assert(task_created);

    // 配置并安装MSC主机驱动
    const msc_host_driver_config_t msc_config = {
        .create_backround_task = true,
        .task_priority = 5,
        .stack_size = 2048,
        .callback = msc_event_cb,
    };
    ESP_ERROR_CHECK( msc_host_install(&msc_config) );

    // 配置FAT文件系统挂载选项
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3,
        .allocation_unit_size = 1024,
    };

    // 主循环，等待USB设备连接并进行文件操作
    do {
        uint8_t device_address = wait_for_msc_device();

        // 安装MSC设备
        ESP_ERROR_CHECK( msc_host_install_device(device_address, &msc_device) );

        // 打印设备描述符
        msc_host_print_descriptors(msc_device);

        // 获取设备信息并打印
        ESP_ERROR_CHECK( msc_host_get_device_info(msc_device, &info) );
        print_device_info(&info);

        // 注册VFS文件系统
        ESP_ERROR_CHECK( msc_host_vfs_register(msc_device, "/usb", &mount_config, &vfs_handle) );

        // 等待设备断开连接，期间执行文件操作
        while (!wait_for_event(DEVICE_DISCONNECTED, 200)) {
            file_operations();
        }

        // 清除事件组位，卸载VFS文件系统和MSC设备
        xEventGroupClearBits(usb_flags, READY_TO_UNINSTALL);
        ESP_ERROR_CHECK( msc_host_vfs_unregister(vfs_handle) );
        ESP_ERROR_CHECK( msc_host_uninstall_device(msc_device) );

    } while (gpio_get_level(USB_DISCONNECT_PIN) != 0); // 当USB断开连接引脚为低电平时退出循环

    // 卸载MSC主机驱动和USB主机库
    ESP_LOGI(TAG, "Uninitializing USB ...");
    ESP_ERROR_CHECK( msc_host_uninstall() );
    wait_for_event(READY_TO_UNINSTALL, portMAX_DELAY);
    ESP_ERROR_CHECK( usb_host_uninstall() );
    ESP_LOGI(TAG, "Done");
}