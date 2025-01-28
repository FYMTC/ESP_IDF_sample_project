#include "usb_uac.h"

#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/uac_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "esp_audio_dec.h"
#include "esp_audio_dec_reg.h"
#include "esp_mp3_dec.h"
#include "audio_player.h"
#include <inttypes.h>
#include "string.h"
// 包含 PRIu32 宏
static const char *TAG = "AUDIO_PLAYER"; // 日志标签
// 定义USB主机任务的优先级为5
#define USB_HOST_TASK_PRIORITY 5
// 定义USB音频类（UAC）任务的优先级为5
#define UAC_TASK_PRIORITY 5
// 定义用户任务的优先级为2
#define USER_TASK_PRIORITY 2

// 定义BIT1_SPK_START宏，用于表示扬声器启动的位掩码，0x01左移0位，即0x01
#define BIT1_SPK_START (0x01 << 0)
// 定义默认音量为30
#define DEFAULT_VOLUME 30
// 定义默认的USB音频类（UAC）采样频率为48000Hz
#define DEFAULT_UAC_FREQ 48000
// 定义默认的USB音频类（UAC）位深度为16位
#define DEFAULT_UAC_BITS 16
// 定义默认的USB音频类（UAC）通道数为2（立体声）
#define DEFAULT_UAC_CH 2
// 定义USB主机任务堆栈大小为3KB
#define USB_HOST_TASK_STACK_SIZE 1024 * 3
// 定义USB音频类（UAC）任务堆栈大小为3KB
#define UAC_TASK_STACK_SIZE 1024 * 3
// 定义音频编解码器任务堆栈大小为4KB
#define codec_TASK_STACK_SIZE 1024 * 4
// 定义USB音频类主机任务堆栈大小为3KB
#define USB_UAC_Host_STACK_SIZE 1024 * 3
// 定义帧缓冲区大小为4KB
#define input_buffer_size 12*1024
#define out_fram_buffer_size 4096

static QueueHandle_t s_event_queue = NULL;          // 事件队列
uac_host_device_handle_t s_spk_dev_handle = NULL;   // USB音频设备句柄
static uint32_t s_spk_curr_freq = DEFAULT_UAC_FREQ; // 当前扬声器采样频率
static uint8_t s_spk_curr_bits = DEFAULT_UAC_BITS;  // 当前扬声器位深度
static uint8_t s_spk_curr_ch = DEFAULT_UAC_CH;      // 当前扬声器通道数
esp_audio_dec_handle_t decoder;
// 定义队列句柄
QueueHandle_t audio_file_queue;
// 定义音量控制队列
QueueHandle_t volume_queue;
// 全局音量值
uint8_t current_volume = 30;

// 全局变量，用于控制当前播放状态
volatile bool is_playing = false;
volatile bool stop_current_playback = false;

// USB音频设备回调函数声明
static void uac_device_callback(uac_host_device_handle_t uac_device_handle, const uac_host_device_event_t event, void *arg);

/**
 * @brief 事件组枚举
 *
 * APP_EVENT            - 通用控制事件
 * UAC_DRIVER_EVENT     - UAC主机驱动事件，如设备连接
 * UAC_DEVICE_EVENT     - UAC主机设备事件，如接收/发送完成，设备断开
 */
typedef enum
{
    APP_EVENT = 0,
    UAC_DRIVER_EVENT,
    UAC_DEVICE_EVENT,
} event_group_t;

/**
 * @brief 事件队列结构体
 *
 * 用于将UAC主机事件从回调函数传递到uac_lib_task
 */
typedef struct
{
    event_group_t event_group; // 事件组
    union
    {
        struct
        {
            uint8_t addr;                  // 设备地址
            uint8_t iface_num;             // 接口号
            uac_host_driver_event_t event; // 驱动事件
            void *arg;                     // 参数
        } driver_evt;                      // 驱动事件结构体
        struct
        {
            uac_host_device_handle_t handle; // 设备句柄
            uac_host_driver_event_t event;   // 驱动事件
            void *arg;                       // 参数
        } device_evt;                        // 设备事件结构体
    };
} s_event_queue_t;

/**
 * @brief 音频播放器静音功能
 *
 * @param setting 静音设置
 * @return esp_err_t 返回ESP_OK表示成功，否则返回错误码
 */
static esp_err_t _audio_player_mute_fn(AUDIO_PLAYER_MUTE_SETTING setting)
{
    if (s_spk_dev_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE; // 设备未连接，返回无效状态
    }
    ESP_LOGI(TAG, "mute setting: %s", setting == AUDIO_PLAYER_MUTE ? "mute" : "unmute");
    // 某些UAC设备可能不支持静音，因此不检查返回值
    if (setting == AUDIO_PLAYER_UNMUTE)
    {
        uac_host_device_set_volume(s_spk_dev_handle, DEFAULT_VOLUME); // 设置音量
        uac_host_device_set_mute(s_spk_dev_handle, false);            // 取消静音
    }
    else
    {
        uac_host_device_set_volume(s_spk_dev_handle, 0);  // 设置音量为0
        uac_host_device_set_mute(s_spk_dev_handle, true); // 静音
    }
    return ESP_OK;
}

/**
 * @brief 音频播放器写数据功能
 *
 * @param audio_buffer 音频数据缓冲区
 * @param len 数据长度
 * @param bytes_written 实际写入的字节数
 * @param timeout_ms 超时时间
 * @return esp_err_t 返回ESP_OK表示成功，否则返回错误码
 */
static esp_err_t _audio_player_write_fn(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    if (s_spk_dev_handle == NULL)
    {
        return ESP_ERR_INVALID_STATE; // 设备未连接，返回无效状态
    }
    *bytes_written = 0;
    esp_err_t ret = uac_host_device_write(s_spk_dev_handle, audio_buffer, len, timeout_ms); // 写入音频数据
    if (ret == ESP_OK)
    {
        *bytes_written = len; // 写入成功，更新写入字节数
    }
    return ret;
}

/**
 * @brief USB音频设备回调函数
 *
 * @param uac_device_handle 设备句柄
 * @param event 事件类型
 * @param arg 参数
 */
static void uac_device_callback(uac_host_device_handle_t uac_device_handle, const uac_host_device_event_t event, void *arg)
{
    if (event == UAC_HOST_DRIVER_EVENT_DISCONNECTED)
    { // 设备断开事件
        // 先停止音频播放器
        s_spk_dev_handle = NULL;
        // audio_player_stop(); // 停止播放
        ESP_LOGI(TAG, "UAC Device disconnected");
        ESP_ERROR_CHECK(uac_host_device_close(uac_device_handle)); // 关闭设备
        return;
    }
    // 将UAC设备事件发送到事件队列
    s_event_queue_t evt_queue = {
        .event_group = UAC_DEVICE_EVENT,
        .device_evt.handle = uac_device_handle,
        .device_evt.event = event,
        .device_evt.arg = arg};
    // 此处不应阻塞
    xQueueSend(s_event_queue, &evt_queue, 0);
}

/**
 * @brief USB主机库回调函数
 *
 * @param addr 设备地址
 * @param iface_num 接口号
 * @param event 事件类型
 * @param arg 参数
 */
static void uac_host_lib_callback(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event, void *arg)
{
    // 将UAC驱动事件发送到事件队列
    s_event_queue_t evt_queue = {
        .event_group = UAC_DRIVER_EVENT,
        .driver_evt.addr = addr,
        .driver_evt.iface_num = iface_num,
        .driver_evt.event = event,
        .driver_evt.arg = arg};
    xQueueSend(s_event_queue, &evt_queue, 0);
}

/**
 * @brief 启动USB主机并处理常见的USB主机库事件
 *
 * @param arg 未使用
 */
static void usb_lib_task(void *arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config)); // 安装USB主机
    ESP_LOGI(TAG, "USB Host installed");
    xTaskNotifyGive(arg); // 通知任务

    while (true)
    {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags); // 处理USB主机事件
        // 在此示例中，只有一个客户端注册
        // 因此，一旦我们注销客户端，此调用必须成功返回ESP_OK
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_ERROR_CHECK(usb_host_device_free_all()); // 释放所有设备
            break;
        }
    }

    ESP_LOGI(TAG, "USB Host shutdown");
    // 清理USB主机
    vTaskDelay(10);                        // 短暂延迟以允许客户端清理
    ESP_ERROR_CHECK(usb_host_uninstall()); // 卸载USB主机
    vTaskDelete(NULL);                     // 删除任务
}

/**
 * @brief UAC库任务
 *
 * @param arg 未使用
 */
static void uac_lib_task(void *arg)
{
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // 等待通知
    uac_host_driver_config_t uac_config = {
        .create_background_task = true,
        .task_priority = UAC_TASK_PRIORITY,
        .stack_size = USB_UAC_Host_STACK_SIZE,
        .core_id = 0,
        .callback = uac_host_lib_callback,
        .callback_arg = NULL};

    ESP_ERROR_CHECK(uac_host_install(&uac_config)); // 安装UAC驱动
    ESP_LOGI(TAG, "UAC Class Driver installed");
    s_event_queue_t evt_queue = {0};
    while (1)
    {
        if (xQueueReceive(s_event_queue, &evt_queue, portMAX_DELAY))
        { // 接收事件
            if (UAC_DRIVER_EVENT == evt_queue.event_group)
            { // 驱动事件
                uac_host_driver_event_t event = evt_queue.driver_evt.event;
                uint8_t addr = evt_queue.driver_evt.addr;
                uint8_t iface_num = evt_queue.driver_evt.iface_num;
                switch (event)
                {
                case UAC_HOST_DRIVER_EVENT_TX_CONNECTED:
                { // 发送连接事件
                    uac_host_dev_info_t dev_info;
                    uac_host_device_handle_t uac_device_handle = NULL;
                    const uac_host_device_config_t dev_config = {
                        .addr = addr,
                        .iface_num = iface_num,
                        .buffer_size = 16000,     ///////////////////////16000
                        .buffer_threshold = 4000*2, ///////////////////4000
                        .callback = uac_device_callback,
                        .callback_arg = NULL,
                    };
                    ESP_ERROR_CHECK(uac_host_device_open(&dev_config, &uac_device_handle));  // 打开设备
                    ESP_ERROR_CHECK(uac_host_get_device_info(uac_device_handle, &dev_info)); // 获取设备信息
                    ESP_LOGI(TAG, "UAC Device connected: SPK");
                    uac_host_printf_device_param(uac_device_handle); // 打印设备参数
                    // 使用默认配置启动USB扬声器
                    const uac_host_stream_config_t stm_config = {
                        .channels = s_spk_curr_ch,
                        .bit_resolution = s_spk_curr_bits,
                        .sample_freq = s_spk_curr_freq,
                    };
                    esp_err_t err = uac_host_device_start(uac_device_handle, &stm_config); // 启动设备
                    if (err == ESP_ERR_NOT_SUPPORTED)
                    {
                        ESP_LOGE(TAG, "Unable to claim Interface, error: %s", esp_err_to_name(err)); // 接口不支持
                        // 处理错误，可能重试或中止操作
                        return;
                    }
                    ESP_ERROR_CHECK(err);
                    s_spk_dev_handle = uac_device_handle; // 更新设备句柄
                    // xQueueSend(audio_file_queue, MOUNT_POINT MP3_FILE_NAME, portMAX_DELAY);// 发送文件路径

                    break;
                }
                case UAC_HOST_DRIVER_EVENT_RX_CONNECTED:
                { // 接收连接事件
                    // 此示例不支持麦克风
                    ESP_LOGI(TAG, "UAC Device connected: MIC");
                    break;
                }
                default:
                    break;
                }
            }
            else if (UAC_DEVICE_EVENT == evt_queue.event_group)
            { // 设备事件
                uac_host_device_event_t event = evt_queue.device_evt.event;
                switch (event)
                {
                case UAC_HOST_DRIVER_EVENT_DISCONNECTED: // 设备断开事件
                    s_spk_curr_bits = DEFAULT_UAC_BITS;  // 重置位深度
                    s_spk_curr_freq = DEFAULT_UAC_FREQ;  // 重置采样率
                    s_spk_curr_ch = DEFAULT_UAC_CH;      // 重置通道数
                    ESP_LOGI(TAG, "UAC Device disconnected");
                    break;
                case UAC_HOST_DEVICE_EVENT_RX_DONE: // 接收完成事件
                    break;
                case UAC_HOST_DEVICE_EVENT_TX_DONE: // 发送完成事件
                    break;
                case UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR: // 传输错误事件
                    break;
                default:
                    break;
                }
            }
            else if (APP_EVENT == evt_queue.event_group)
            { // 应用事件
                break;
            }
        }
    }

    ESP_LOGI(TAG, "UAC Driver uninstall");
    ESP_ERROR_CHECK(uac_host_uninstall()); // 卸载UAC驱动
}

// 根据文件扩展名获取音频类型
esp_audio_type_t get_audio_type_from_file(const char *file_path)
{
    const char *ext = strrchr(file_path, '.');
    if (ext == NULL)
    {
        return ESP_AUDIO_TYPE_UNSUPPORT;
    }

    if (strcmp(ext, ".aac") == 0)
    {
        return ESP_AUDIO_TYPE_AAC;
    }
    else if (strcmp(ext, ".mp3") == 0)
    {
        return ESP_AUDIO_TYPE_MP3;
    }
    else
    {
        return ESP_AUDIO_TYPE_UNSUPPORT;
    }
}

// 音频解码和播放任务
void audio_player_task(void *pvParameters)
{
    // 注册 MP3 解码器
    esp_mp3_dec_register();

    while (1)
    {
        char file_path[256];
        if (xQueueReceive(audio_file_queue, &file_path, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "Received file path: %s", file_path);

            // 根据文件扩展名选择解码器类型
            esp_audio_type_t audio_type = get_audio_type_from_file(file_path);
            if (audio_type == ESP_AUDIO_TYPE_UNSUPPORT)
            {
                ESP_LOGE(TAG, "Unsupported audio format: %s", file_path);
                continue;
            }

            // 配置解码器
            esp_audio_dec_cfg_t dec_cfg = {
                .type = audio_type, // 根据文件扩展名设置解码器类型
                .cfg = NULL,        // 如果没有特殊配置，设置为 NULL
                .cfg_sz = 0         // 如果没有特殊配置，设置为 0
            };

            esp_audio_dec_handle_t decoder;
            esp_audio_err_t ret;

            // 1. 打开解码器
            ret = esp_audio_dec_open(&dec_cfg, &decoder);
            if (ret != ESP_AUDIO_ERR_OK)
            {
                ESP_LOGE(TAG, "Failed to open audio decoder, error: %d", ret);
                continue;
            }

            // 2. 准备输入数据和输出缓冲区
            uint8_t *input_buffer = (uint8_t *)heap_caps_malloc(input_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT); // 输入缓冲区
            uint8_t *frame_data = (uint8_t *)heap_caps_malloc(out_fram_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);   // 初始输出缓冲区
            esp_audio_dec_out_frame_t out_frame = {.buffer = frame_data, .len = out_fram_buffer_size};

            uac_host_device_set_volume(s_spk_dev_handle, current_volume); // 设置音量
            // 打开文件
            FILE *file = fopen(file_path, "rb");
            if (file == NULL)
            {
                ESP_LOGE(TAG, "Failed to open file: %s", file_path);
                continue;
            }
            esp_audio_dec_in_raw_t raw;
            raw.len=0;
            // 定义一个临时缓冲区，用于保存未解码的数据
            uint8_t *temp_buffer = (uint8_t *)heap_caps_malloc(input_buffer_size+ out_fram_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT); // 假设最大为两倍的帧缓冲区大小
            uint32_t temp_buffer_len = 0;                                                                                     // 临时缓冲区中未解码数据的长度
            while (1)
            {
                if (raw.len < out_fram_buffer_size)
                {
                    // 从文件中读取一段数据
                    size_t bytes_read = fread(input_buffer, 1, input_buffer_size, file);

                    if (ferror(file))
                    {
                        ESP_LOGE(TAG, "Error reading file: %s", file_path);
                        break;
                    }
                    // 如果有未解码的数据，将其与新的输入数据拼接
                    memcpy(temp_buffer + temp_buffer_len, input_buffer, bytes_read);
                    raw.buffer = temp_buffer;
                    raw.len = temp_buffer_len + bytes_read;
                    ESP_LOGI(TAG, "Read %zu, temp_buffer_len: %lu, raw.len: %lu", bytes_read, temp_buffer_len, raw.len);
                }
                // 解码数据并播放
                //  while (raw.len)
                //  {
                ret = esp_audio_dec_process(decoder, &raw, &out_frame);
                if (ret != ESP_AUDIO_ERR_OK && ret != ESP_AUDIO_ERR_BUFF_NOT_ENOUGH)
                {
                    ESP_LOGE(TAG, "Failed to process audio data, error: %d", ret);
                    break;
                }
                // 更新输入数据指针和长度
                raw.buffer += raw.consumed;
                raw.len -= raw.consumed;
                ESP_LOGI(TAG, "consumed: %lu, raw.len: %lu", raw.consumed, raw.len);
                // 如果有未解码的数据，保存到临时缓冲区
                if (raw.len > 0 && raw.len <=out_fram_buffer_size+input_buffer_size)
                {
                    memcpy(temp_buffer, raw.buffer, raw.len);
                    temp_buffer_len = raw.len;
                    ESP_LOGI(TAG, "reset temp_buffer_len: %lu", temp_buffer_len);
                }
                else
                {
                    ESP_LOGE(TAG, "Invalid raw.len: %lu", raw.len);
                    break;
                }
                if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH)
                {
                    // 输出缓冲区不足，重新分配更大的缓冲区
                    uint8_t *new_frame_data = (uint8_t *)heap_caps_realloc(out_frame.buffer, out_frame.needed_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_DEFAULT);
                    if (new_frame_data == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to realloc output buffer");
                        break;
                    }
                    out_frame.buffer = new_frame_data;
                    out_frame.len = out_frame.needed_size;
                }
                ESP_LOGI(TAG, "decoded_size: %lu", out_frame.decoded_size);
                //}
                esp_err_t write_ret = uac_host_device_write(s_spk_dev_handle, out_frame.buffer, out_frame.decoded_size, portMAX_DELAY);
                if (write_ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to write audio data to device, error: %d", write_ret);
                    break;
                }
                if (feof(file))
                {
                    ESP_LOGI(TAG, "Finished reading file: %s", file_path);
                    break; // 文件读取完毕
                }
            }
            // 关闭文件
            fclose(file);
            // 4. 获取解码器信息
            esp_audio_dec_info_t dec_info;
            ret = esp_audio_dec_get_info(decoder, &dec_info);
            if (ret == ESP_AUDIO_ERR_OK)
            {
                ESP_LOGI(TAG, "Sample rate: %" PRIu32 ", Channels: %u, Bits per sample: %u",
                         dec_info.sample_rate, dec_info.channel, dec_info.bits_per_sample);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to get decoder info, error: %d", ret);
            }

            // 5. 关闭解码器
            esp_audio_dec_close(decoder);

            // 释放资源（PSRAM 中的内存）
            heap_caps_free(frame_data);
            heap_caps_free(input_buffer);
            heap_caps_free(temp_buffer);
        }
    }
}
/**
 * @brief 主函数
 */
void uac_init(void)
{
    s_event_queue = xQueueCreate(10, sizeof(s_event_queue_t)); // 创建事件队列
    assert(s_event_queue != NULL);

    // 创建队列
    audio_file_queue = xQueueCreate(5, sizeof(char[256]));
    if (audio_file_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }
    // 创建音量控制队列
    volume_queue = xQueueCreate(5, sizeof(float));
    if (volume_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create volume queue");
        return;
    }

    static TaskHandle_t uac_task_handle = NULL;
    BaseType_t ret = xTaskCreatePinnedToCore(uac_lib_task, "uac_events", UAC_TASK_STACK_SIZE, NULL,
                                             USER_TASK_PRIORITY, &uac_task_handle, 1); // 创建UAC任务
    assert(ret == pdTRUE);
    ret = xTaskCreatePinnedToCore(usb_lib_task, "usb_events", USB_HOST_TASK_STACK_SIZE, (void *)uac_task_handle,
                                  USB_HOST_TASK_PRIORITY, NULL, 1); // 创建USB主机任务
    assert(ret == pdTRUE);

    TaskHandle_t codec_task_handle = NULL;
    xTaskCreate(audio_player_task, "audio_player_task", codec_TASK_STACK_SIZE, NULL, 5, &codec_task_handle);
}