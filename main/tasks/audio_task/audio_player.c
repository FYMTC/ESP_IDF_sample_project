#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs_fat.h"
#include "esp_audio_dec.h"
#include "esp_audio_dec_reg.h"
#include "esp_audio_dec_default.h"
#include "esp_mp3_dec.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "string.h"
#include "usb/uac_host.h"
#include <math.h>
#include "conf.h"

extern uac_host_device_handle_t s_spk_dev_handle;
// extern uint8_t player_volume;
extern float led_brightness;

#ifdef USE_I2S_AUDIO
extern QueueHandle_t i2s_audio_data_queue;
#endif

#ifdef USE_UAC_AUDIO
extern QueueHandle_t uac_audio_data_queue;
#endif

bool player_playing = false;
bool decoder_closed = true;
static const char *TAG = "PLAYER";
// 音频解码器句柄
esp_audio_dec_handle_t decoder;
// 音乐文件 队列句柄
QueueHandle_t audio_file_queue;
// 控制解码器播放文件的队列
QueueHandle_t audio_control_file_queue;
// 定义缓冲区大小
#define head_buffer_size 1024 * 500 // 缓存音乐文件头部信息，可能包含图片，所以存大一点
#define input_buffer_size 1024 * 12
#define out_fram_buffer_size 1024 * 4
// 定义音频任务堆栈大小
#define codec_TASK_STACK_SIZE 1024 * 3
#define codec_countrol_TASK_STACK_SIZE 1024 * 3

static uint8_t get_volume_from_nvs()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for reading!");
        return 50; 
    }

    uint8_t volume = 50; 
    err = nvs_get_u8(my_handle, BRIGHTNESS_KEY, &volume);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read brightness from NVS, using default.");
        volume = 50; 
    }

    nvs_close(my_handle);
    
    return volume;
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

// 计算音频帧的音量（RMS 值）
void calculate_brightness(uint8_t *pcm_data, size_t pcm_length, float *brightness)
{
    float sum = 0.0f;
    for (size_t i = 0; i < pcm_length; i++)
    {
        sum += (float)((pcm_data[i]) * (pcm_data[i]));
    }
    float volume = sqrtf(sum / pcm_length); // 计算 RMS（幅值的均方根）
    //printf("%f\n",volume);
    //*brightness = ((volume / 327.68f) - 0.45) * 10;//不太闪
    *brightness = (volume -145) * 0.05;//闪瞎眼

    //更快地算法
    // for (size_t i = 0; i < pcm_length; i++)
    // {
    //     sum += (float)(pcm_data[i]); // 直接累加 PCM 数据
    // }
    // *brightness = sum / pcm_length; // 直接使用平均值
    // //printf("%f\n",*brightness);
    // *brightness = (*brightness -118) * 0.05;
    if (*brightness > 1.0f)
        *brightness = 1.0f;
    if (*brightness < 0)
        *brightness = 0;
}

void audio_get_volume( uint8_t *volume)
{
    #ifdef USE_UAC_AUDIO
    uac_host_device_get_volume(s_spk_dev_handle, volume);
    #endif
}
void audio_set_volume(uint8_t volume)
{
    #ifdef USE_UAC_AUDIO
    uac_host_device_set_volume(s_spk_dev_handle, volume);
    #endif
    
}

void audio_set_mute(bool mute)
{
    #ifdef USE_UAC_AUDIO
    uac_host_device_set_mute(s_spk_dev_handle, mute);
    #endif
}

void audio_decoder_task(void *pvParameters)
{
    // 注册解码器
    esp_audio_dec_register_default();
    char file_path[256];
    while (1)
    {
        if (xQueueReceive(audio_control_file_queue, &file_path, portMAX_DELAY) == pdTRUE)
        {
            player_playing = true;
            decoder_closed = false;
            audio_set_mute(false);
            audio_set_volume(get_volume_from_nvs());
            ESP_LOGI(TAG, "Received file path: %s", file_path);

            // 根据文件扩展名选择解码器类型
            esp_audio_type_t audio_type = get_audio_type_from_file(file_path);
            if (audio_type == ESP_AUDIO_TYPE_UNSUPPORT)
            {
                ESP_LOGE(TAG, "Unsupported audio format: %s", file_path);
                player_playing = false;
                decoder_closed = true;
                audio_set_mute(true);
                continue;
            }

            // 配置解码器
            esp_audio_dec_cfg_t dec_cfg = {
                .type = audio_type, // 根据文件扩展名设置解码器类型
                .cfg = NULL,        // 如果没有特殊配置，设置为 NULL
                .cfg_sz = 0         // 如果没有特殊配置，设置为 0
            };

            esp_audio_dec_handle_t decoder = NULL;
            // 1. 打开解码器
            esp_audio_err_t ret = esp_audio_dec_open(&dec_cfg, &decoder);
            if (ret != ESP_AUDIO_ERR_OK)
            {
                ESP_LOGE(TAG, "Failed to open audio decoder, error: %d", ret);
                player_playing = false;
                decoder_closed = true;
                audio_set_mute(true);
                continue;
            }
            // 打开文件
            FILE *file = fopen(file_path, "rb");
            if (file == NULL)
            {
                ESP_LOGE(TAG, "Failed to open file: %s", file_path);
                player_playing = false;
                decoder_closed = true;
                audio_set_mute(true);
                continue;
            }
            // 2. 准备输入数据和输出缓冲区
            uint8_t *input_buffer = (uint8_t *)heap_caps_malloc(input_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);  // 输入缓冲区
            uint8_t *frame_data = (uint8_t *)heap_caps_malloc(out_fram_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); // 初始输出缓冲区
            uint8_t *temp_buffer = (uint8_t *)heap_caps_malloc(input_buffer_size * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            uint8_t *head_buffer = (uint8_t *)heap_caps_malloc(head_buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

            uint32_t temp_buffer_len = 0;
            size_t bytes_read = 0;
            esp_audio_dec_in_raw_t raw;
            while (1)
            {
                if (bytes_read == 0)
                {
                    bytes_read = fread(head_buffer, 1, head_buffer_size, file);
                    if (ferror(file))
                    {
                        ESP_LOGE(TAG, "Error reading file: %s", file_path);
                        break;
                    }

                    raw.buffer = head_buffer;
                    raw.len = bytes_read;
                }
                else
                {
                    bytes_read = fread(input_buffer, 1, input_buffer_size, file);
                    if (ferror(file))
                    {
                        ESP_LOGE(TAG, "Error reading file: %s", file_path);
                        break;
                    }
                    memcpy(temp_buffer + temp_buffer_len, input_buffer, bytes_read);

                    raw.buffer = temp_buffer;
                    raw.len = temp_buffer_len + bytes_read;
                }
                esp_audio_dec_out_frame_t out_frame = {
                    .buffer = frame_data,
                    .len = out_fram_buffer_size,
                };
                //ESP_LOGI(TAG, "Read %zu, temp_buffer_len: %lu, raw.len: %lu", bytes_read, temp_buffer_len, raw.len);
                //  解码数据并放入队列
                while (raw.len > 1440)
                {
                    ret = esp_audio_dec_process(decoder, &raw, &out_frame);
                    if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH)
                    {
                        // 输出缓冲区不足，重新分配更大的缓冲区
                        uint8_t *new_frame_data = (uint8_t *)heap_caps_realloc(frame_data, out_frame.needed_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
                        if (new_frame_data == NULL)
                        {
                            ESP_LOGE(TAG, "Failed to realloc output buffer");
                            break;
                        }
                        frame_data = new_frame_data;
                        out_frame.buffer = new_frame_data;
                        out_frame.len = out_frame.needed_size;
                        ret = ESP_AUDIO_ERR_CONTINUE;
                        continue;
                    }

                    if (ret != ESP_AUDIO_ERR_OK)
                    {
                        ESP_LOGE(TAG, "Failed to process audio data, error: %d", ret);
                        break;
                    }

                    #ifdef USE_UAC_AUDIO
                    // 将解码后的数据放入UAC音频播放队列
                    if (xQueueSend(uac_audio_data_queue, &out_frame, pdMS_TO_TICKS(1000)) != pdTRUE)
                    {
                        ESP_LOGE(TAG, "Failed to send audio data to queue");
                        break;
                    }
                    #endif

                    #ifdef USE_I2S_AUDIO
                    // 将解码后的数据放入I2S音频播放队列
                    if (xQueueSend(i2s_audio_data_queue, &out_frame, pdMS_TO_TICKS(1000)) != pdTRUE)
                    {
                        ESP_LOGE(TAG, "Failed to send audio data to queue");
                        break;
                    }
                    #endif

                    // 更新输入数据指针和长度
                    raw.buffer += raw.consumed;
                    raw.len -= raw.consumed;
                    //ESP_LOGI(TAG, "consumed: %lu, raw.len: %lu", raw.consumed, raw.len);

                    // 计算当前帧的音量
                    calculate_brightness(out_frame.buffer, out_frame.decoded_size, &led_brightness);
                    

                    if (!player_playing)
                    {
                        ESP_LOGI(TAG, "Player STOP play");
                        break;
                    }
                }
                if (!player_playing)
                {
                    break;
                }
                if (raw.len > 0 && raw.len <= out_fram_buffer_size)
                {
                    memcpy(temp_buffer, raw.buffer, raw.len);
                    temp_buffer_len = raw.len;
                    //ESP_LOGI(TAG, "reset temp_buffer_len: %lu", temp_buffer_len);
                }
                else if (raw.len == 0)
                {
                    ESP_LOGI(TAG, "Finished process");
                    break;
                }
                else
                {
                    ESP_LOGE(TAG, "Invalid raw.len: %lu", raw.len);
                    break;
                }
                if (feof(file))
                {
                    ESP_LOGI(TAG, "Finished reading file: %s", file_path);
                    break; // 文件读取完毕
                }
            }
            audio_set_mute(true);
            //  关闭文件
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
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延迟一下再清理，以免出现噪音

            // 释放资源（PSRAM 中的内存
            if (head_buffer)
            {
                heap_caps_free(head_buffer);
            }
            if (frame_data)
            {
                heap_caps_free(frame_data);
            }
            if (input_buffer)
            {
                heap_caps_free(input_buffer);
            }
            if (temp_buffer)
            {
                heap_caps_free(temp_buffer);
            }
            player_playing = false;
            decoder_closed = true;
        }
    }
}
void codec_control_task(void *pvParameters)
{
    char new_file_path[256];
    while (1)
    {
        if (xQueueReceive(audio_file_queue, &new_file_path, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "audio_control_task Received new file path: %s,player_playing:%s", new_file_path, (player_playing ? "true" : "false"));

            if (player_playing)
            {

                uint8_t volume=30;
                audio_get_volume(&volume);
                for (uint8_t v = volume; v > 0; v -= 1)// 渐出效果，逐渐降低音量
                {
                    audio_set_volume(v);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
                audio_set_mute(true);
                player_playing = false;
            }
            ESP_LOGI(TAG, "decoder_closed:%s", (decoder_closed ? "true" : "false"));
            // Send the new file path to the decoder task
            if (xQueueSend(audio_control_file_queue, &new_file_path, pdMS_TO_TICKS(2000)) != pdTRUE)
            {
                ESP_LOGE(TAG, "Failed to send %s to audio_control_file_queue", new_file_path);
            }
        }
    }
}
void audio_player_init(void)
{
    // 创建音乐文件队列
    audio_file_queue = xQueueCreate(5, sizeof(char[256]));
    if (audio_file_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    audio_control_file_queue = xQueueCreate(5, sizeof(char[256]));
    if (audio_control_file_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    TaskHandle_t decoder_task_handle = NULL;
    TaskHandle_t control_task_handle = NULL;
    // 创建解码任务
    xTaskCreatePinnedToCore(audio_decoder_task, "audio_decoder_task", codec_TASK_STACK_SIZE, NULL, 3, &decoder_task_handle, 1);
    // 创建解码控制任务
    xTaskCreatePinnedToCore(codec_control_task, "codec_control_task", codec_countrol_TASK_STACK_SIZE, NULL, 5, &control_task_handle, 1);
}