/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "hid_host.h"
#include "bt.hpp"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "errno.h"
#include "driver/gpio.h"

#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"

extern mouse_t bt_mouse_indev;
// 定义用于退出示例逻辑的GPIO引脚
#define APP_QUIT_PIN                GPIO_NUM_0

// 日志标签
static const char *TAG = "example";

// HID主机事件队列
QueueHandle_t hid_host_event_queue;

// 用户关闭标志
bool user_shutdown = false;

/**
 * @brief HID主机事件结构体
 *
 * 该结构体用于将HID主机事件从回调传递到任务。
 */
typedef struct {
    hid_host_device_handle_t hid_device_handle;  // HID设备句柄
    hid_host_driver_event_t event;               // HID主机驱动事件
    void *arg;                                   // 参数
} hid_host_event_queue_t;

/**
 * @brief HID协议字符串名称
 */
static const char *hid_proto_name_str[] = {
    "NONE",     // 无协议
    "KEYBOARD", // 键盘协议
    "MOUSE"     // 鼠标协议
};

/**
 * @brief 键盘事件结构体
 */
typedef struct {
    enum key_state {
        KEY_STATE_PRESSED = 0x00,  // 按键按下
        KEY_STATE_RELEASED = 0x01  // 按键释放
    } state;                       // 按键状态
    uint8_t modifier;              // 修饰键
    uint8_t key_code;              // 键码
} key_event_t;

// 回车键的主字符
#define KEYBOARD_ENTER_MAIN_CHAR    '\r'

// 当设置为1时，按下回车键将在串口调试输出中扩展换行符
#define KEYBOARD_ENTER_LF_EXTEND    1

/**
 * @brief 键码到ASCII码的映射表
 */
const uint8_t keycode2ascii [57][2] = {
    {0, 0}, /* HID_KEY_NO_PRESS        */
    {0, 0}, /* HID_KEY_ROLLOVER        */
    {0, 0}, /* HID_KEY_POST_FAIL       */
    {0, 0}, /* HID_KEY_ERROR_UNDEFINED */
    {'a', 'A'}, /* HID_KEY_A               */
    {'b', 'B'}, /* HID_KEY_B               */
    {'c', 'C'}, /* HID_KEY_C               */
    {'d', 'D'}, /* HID_KEY_D               */
    {'e', 'E'}, /* HID_KEY_E               */
    {'f', 'F'}, /* HID_KEY_F               */
    {'g', 'G'}, /* HID_KEY_G               */
    {'h', 'H'}, /* HID_KEY_H               */
    {'i', 'I'}, /* HID_KEY_I               */
    {'j', 'J'}, /* HID_KEY_J               */
    {'k', 'K'}, /* HID_KEY_K               */
    {'l', 'L'}, /* HID_KEY_L               */
    {'m', 'M'}, /* HID_KEY_M               */
    {'n', 'N'}, /* HID_KEY_N               */
    {'o', 'O'}, /* HID_KEY_O               */
    {'p', 'P'}, /* HID_KEY_P               */
    {'q', 'Q'}, /* HID_KEY_Q               */
    {'r', 'R'}, /* HID_KEY_R               */
    {'s', 'S'}, /* HID_KEY_S               */
    {'t', 'T'}, /* HID_KEY_T               */
    {'u', 'U'}, /* HID_KEY_U               */
    {'v', 'V'}, /* HID_KEY_V               */
    {'w', 'W'}, /* HID_KEY_W               */
    {'x', 'X'}, /* HID_KEY_X               */
    {'y', 'Y'}, /* HID_KEY_Y               */
    {'z', 'Z'}, /* HID_KEY_Z               */
    {'1', '!'}, /* HID_KEY_1               */
    {'2', '@'}, /* HID_KEY_2               */
    {'3', '#'}, /* HID_KEY_3               */
    {'4', '$'}, /* HID_KEY_4               */
    {'5', '%'}, /* HID_KEY_5               */
    {'6', '^'}, /* HID_KEY_6               */
    {'7', '&'}, /* HID_KEY_7               */
    {'8', '*'}, /* HID_KEY_8               */
    {'9', '('}, /* HID_KEY_9               */
    {'0', ')'}, /* HID_KEY_0               */
    {KEYBOARD_ENTER_MAIN_CHAR, KEYBOARD_ENTER_MAIN_CHAR}, /* HID_KEY_ENTER           */
    {0, 0}, /* HID_KEY_ESC             */
    {'\b', 0}, /* HID_KEY_DEL             */
    {0, 0}, /* HID_KEY_TAB             */
    {' ', ' '}, /* HID_KEY_SPACE           */
    {'-', '_'}, /* HID_KEY_MINUS           */
    {'=', '+'}, /* HID_KEY_EQUAL           */
    {'[', '{'}, /* HID_KEY_OPEN_BRACKET    */
    {']', '}'}, /* HID_KEY_CLOSE_BRACKET   */
    {'\\', '|'}, /* HID_KEY_BACK_SLASH      */
    {'\\', '|'}, /* HID_KEY_SHARP           */  // HOTFIX: for NonUS Keyboards repeat HID_KEY_BACK_SLASH
    {';', ':'}, /* HID_KEY_COLON           */
    {'\'', '"'}, /* HID_KEY_QUOTE           */
    {'`', '~'}, /* HID_KEY_TILDE           */
    {',', '<'}, /* HID_KEY_LESS            */
    {'.', '>'}, /* HID_KEY_GREATER         */
    {'/', '?'} /* HID_KEY_SLASH           */
};

/**
 * @brief 根据报告输出协议类型打印新设备报告头
 *
 * @param[in] proto 当前输出协议
 */
static void hid_print_new_device_report_header(hid_protocol_t proto)
{
    static hid_protocol_t prev_proto_output = -1;

    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        printf("\r\n");
        if (proto == HID_PROTOCOL_MOUSE) {
            printf("Mouse\r\n");
        } else if (proto == HID_PROTOCOL_KEYBOARD) {
            printf("Keyboard\r\n");
        } else {
            printf("Generic\r\n");
        }
        fflush(stdout);
    }
}

/**
 * @brief 检查键盘修饰键是否为Shift键（左Shift或右Shift）
 *
 * @param[in] modifier 修饰键
 * @return true  修饰键是Shift键
 * @return false 修饰键不是Shift键
 */
static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier)
{
    if (((modifier & HID_LEFT_SHIFT) == HID_LEFT_SHIFT) ||
            ((modifier & HID_RIGHT_SHIFT) == HID_RIGHT_SHIFT)) {
        return true;
    }
    return false;
}

/**
 * @brief 从键码获取字符符号
 *
 * @param[in] modifier  键盘修饰键
 * @param[in] key_code  键盘键码
 * @param[in] key_char  指向字符数据的指针
 *
 * @return true  键码成功转换为字符
 * @return false 键码未知
 */
static inline bool hid_keyboard_get_char(uint8_t modifier,
        uint8_t key_code,
        unsigned char *key_char)
{
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;

    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
        *key_char = keycode2ascii[key_code][mod];
    } else {
        // 其他键按下
        return false;
    }

    return true;
}

/**
 * @brief 打印键盘字符符号
 *
 * @param[in] key_char  要打印的键盘字符
 */
static inline void hid_keyboard_print_char(unsigned int key_char)
{
    if (!!key_char) {
        putchar(key_char);
#if (KEYBOARD_ENTER_LF_EXTEND)
        if (KEYBOARD_ENTER_MAIN_CHAR == key_char) {
            putchar('\n');
        }
#endif // KEYBOARD_ENTER_LF_EXTEND
        fflush(stdout);
    }
}

/**
 * @brief 键盘事件回调函数
 *
 * @param[in] key_event 指向键盘事件结构体的指针
 */
static void key_event_callback(key_event_t *key_event)
{
    unsigned char key_char;

    hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

    if (KEY_STATE_PRESSED == key_event->state) {
        if (hid_keyboard_get_char(key_event->modifier,
                                  key_event->key_code, &key_char)) {

            hid_keyboard_print_char(key_char);

        }
    }
}

/**
 * @brief 在缓冲区中查找键码
 *
 * @param[in] src       源缓冲区
 * @param[in] key       要查找的键码
 * @param[in] length    源缓冲区长度
 */
static inline bool key_found(const uint8_t *const src,
                             uint8_t key,
                             unsigned int length)
{
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief USB HID主机键盘接口报告回调处理函数
 *
 * @param[in] data    输入报告数据缓冲区
 * @param[in] length  输入报告数据缓冲区长度
 */
static void hid_host_keyboard_report_callback(const uint8_t *const data, const int length)
{
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t *)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = { 0 };
    key_event_t key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {

        // 检查按键是否释放
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        // 检查按键是否按下
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
                !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID主机鼠标接口报告回调处理函数
 *
 * @param[in] data    输入报告数据缓冲区
 * @param[in] length  输入报告数据缓冲区长度
 */
static void hid_host_mouse_report_callback(const uint8_t *const data, const int length)
{
    hid_mouse_input_report_boot_t *mouse_report = (hid_mouse_input_report_boot_t *)data;

    if (length < sizeof(hid_mouse_input_report_boot_t)) {
        return;
    }

    

    bt_mouse_indev.left_button_pressed =(mouse_report->buttons.button1 ? true : false);
    bt_mouse_indev.right_button_pressed =(mouse_report->buttons.button2 ? true : false);
    bt_mouse_indev.x_movement =mouse_report->x_displacement;
    bt_mouse_indev.y_movement =mouse_report->y_displacement;
    bt_mouse_indev.data_frame++;

    // 计算绝对位置
    // static int x_pos = 0;
    // static int y_pos = 0;
    // x_pos += mouse_report->x_displacement;
    // y_pos += mouse_report->y_displacement;



    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);

    // printf("X: %06d\tY: %06d\t|%c|%c|\r",
    //        x_pos, y_pos,
    //        (mouse_report->buttons.button1 ? 'o' : ' '),
    //        (mouse_report->buttons.button2 ? 'o' : ' '));
    // fflush(stdout);
}

/**
 * @brief USB HID主机通用接口报告回调处理函数
 *
 * '通用'指的是除鼠标和键盘之外的设备
 *
 * @param[in] data    输入报告数据缓冲区
 * @param[in] length  输入报告数据缓冲区长度
 */
static void hid_host_generic_report_callback(const uint8_t *const data, const int length)
{
    hid_print_new_device_report_header(HID_PROTOCOL_NONE);
    for (int i = 0; i < length; i++) {
        printf("%02X", data[i]);
    }
    putchar('\r');
}

/**
 * @brief USB HID主机接口回调函数
 *
 * @param[in] hid_device_handle  HID设备句柄
 * @param[in] event              HID主机接口事件
 * @param[in] arg                参数，未使用
 */
// 定义一个回调函数，用于处理HID主机接口的事件
void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg)
{
    // 定义一个64字节的数组用于存储数据
    uint8_t data[64] = { 0 };
    // 定义一个变量用于存储数据的长度
    size_t data_length = 0;
    // 定义一个结构体用于存储设备的参数
    hid_host_dev_params_t dev_params;
    // 获取设备的参数，并检查是否成功
    ESP_ERROR_CHECK( hid_host_device_get_params(hid_device_handle, &dev_params));

    // 根据事件类型进行不同的处理
    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        // 获取原始的输入报告数据，并检查是否成功
        ESP_ERROR_CHECK( hid_host_device_get_raw_input_report_data(hid_device_handle,
                         data,
                         64,
                         &data_length));

        // 判断设备是否为引导接口
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            // 判断设备是否为键盘协议
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                // 调用键盘报告回调函数
                hid_host_keyboard_report_callback(data, data_length);
            // 判断设备是否为鼠标协议
            } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
                // 调用鼠标报告回调函数
                hid_host_mouse_report_callback(data, data_length);
            }
        } else {
            // 调用通用报告回调函数
            hid_host_generic_report_callback(data, data_length);
        }

        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        // 打印设备断开连接的信息
        ESP_LOGI(TAG, "HID Device, protocol '%s' DISCONNECTED",
                 hid_proto_name_str[dev_params.proto]);
        // 关闭设备，并检查是否成功
        ESP_ERROR_CHECK( hid_host_device_close(hid_device_handle) );
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        // 打印设备传输错误的信息
        ESP_LOGI(TAG, "HID Device, protocol '%s' TRANSFER_ERROR",
                 hid_proto_name_str[dev_params.proto]);
        break;
    default:
        // 打印未处理的事件信息
        ESP_LOGE(TAG, "HID Device, protocol '%s' Unhandled event",
                 hid_proto_name_str[dev_params.proto]);
        break;
    }
}

/**
 * @brief USB HID主机设备事件回调函数
 *
 * @param[in] hid_device_handle  HID设备句柄
 * @param[in] event              HID主机设备事件
 * @param[in] arg                参数，未使用
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg)
{
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK( hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol '%s' CONNECTED",
                 hid_proto_name_str[dev_params.proto]);

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };

        ESP_ERROR_CHECK( hid_host_device_open(hid_device_handle, &dev_config) );
        if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
            ESP_ERROR_CHECK( hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
            if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
                ESP_ERROR_CHECK( hid_class_request_set_idle(hid_device_handle, 0, 0));
            }
        }
        ESP_ERROR_CHECK( hid_host_device_start(hid_device_handle) );
        break;
    default:
        break;
    }
}

/**
 * @brief USB主机库任务
 *
 * 初始化USB主机库并处理USB主机事件，直到APP引脚为低电平
 *
 * @param[in] arg  未使用
 */
static void usb_lib_task(void *arg)
{
    const gpio_config_t input_pin = {
        .pin_bit_mask = BIT64(APP_QUIT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK( gpio_config(&input_pin) );

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK( usb_host_install(&host_config) );
    xTaskNotifyGive(arg);

    while (gpio_get_level(APP_QUIT_PIN) != 0) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);

        // 在所有客户端注销后释放设备
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
            ESP_LOGI(TAG, "USB Event flags: NO_CLIENTS");
        }
        // 所有设备已移除
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            ESP_LOGI(TAG, "USB Event flags: ALL_FREE");
        }
    }
    // APP按钮按下，触发关闭标志
    user_shutdown = true;
    ESP_LOGI(TAG, "USB shutdown");
    // 清理USB主机
    vTaskDelay(10); // 短暂延迟以允许客户端清理
    ESP_ERROR_CHECK( usb_host_uninstall());
    vTaskDelete(NULL);
}

/**
 * @brief HID主机任务
 *
 * 创建队列并从队列中获取新事件
 *
 * @param[in] pvParameters 未使用
 */
void hid_host_task(void *pvParameters)
{
    hid_host_event_queue_t evt_queue;
    // 创建队列
    hid_host_event_queue = xQueueCreate(10, sizeof(hid_host_event_queue_t));

    // 等待队列
    while (!user_shutdown) {
        if (xQueueReceive(hid_host_event_queue, &evt_queue, pdMS_TO_TICKS(50))) {
            hid_host_device_event(evt_queue.hid_device_handle,
                                  evt_queue.event,
                                  evt_queue.arg);
        }
    }

    xQueueReset(hid_host_event_queue);
    vQueueDelete(hid_host_event_queue);
    vTaskDelete(NULL);
}

/**
 * @brief HID主机设备回调函数
 *
 * 将新的HID设备事件放入队列
 *
 * @param[in] hid_device_handle  HID设备句柄
 * @param[in] event              HID设备事件
 * @param[in] arg                未使用
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg)
{
    const hid_host_event_queue_t evt_queue = {
        .hid_device_handle = hid_device_handle,
        .event = event,
        .arg = arg
    };
    xQueueSend(hid_host_event_queue, &evt_queue, 0);
}

/**
 * @brief HID主机示例主函数
 */
void hid_host_main(void)
{
    BaseType_t task_created;
    ESP_LOGI(TAG, "HID Host example");

    /*
    * 创建usb_lib_task任务：
    * - 初始化USB主机库
    * - 处理USB主机事件，直到APP引脚为高电平
    */
    task_created = xTaskCreatePinnedToCore(usb_lib_task,
                                           "usb_events",
                                           4096,
                                           xTaskGetCurrentTaskHandle(),
                                           1, NULL,1);
    assert(task_created == pdTRUE);

    // 等待usb_lib_task的通知以继续
    ulTaskNotifyTake(false, 1000);

    /*
    * HID主机驱动配置
    * - 创建后台任务以处理HID驱动中的低级事件
    * - 提供设备回调以获取新的HID设备连接事件
    */
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        .callback_arg = NULL
    };

    ESP_ERROR_CHECK( hid_host_install(&hid_host_driver_config) );

    // 任务在设备存在时运行（当'user_shutdown'为false时）
    user_shutdown = false;

    /*
    * 创建HID主机任务以处理事件
    * 注意：任务在此处是必要的，因为无法从回调中与USB设备交互。
    */
    task_created = xTaskCreate(&hid_host_task, "hid_task", 4 * 1024, NULL, 1, NULL);
    assert(task_created == pdTRUE);
}