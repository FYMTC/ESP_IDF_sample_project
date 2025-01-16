#if 0

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "tusb.h"
#include "mp3dec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "audio_task.h"

static const char *TAG = "USB_AUDIO_MP3";

// #define MOUNT_POINT "/sdcard"
#define BUFFER_SIZE 1024
#define PCM_BUFFER_SIZE 1152 * 2
#define VOLUME_MIN 0
#define VOLUME_MAX 100

QueueHandle_t pcm_queue;
SemaphoreHandle_t play_control_mutex;
volatile bool is_playing = true;
volatile int volume = 50; // 默认音量

typedef struct {
    int16_t *pcm;
    int samples;
} pcm_buffer_t;

void usb_audio_task(void *pvParameters)
{
    pcm_buffer_t pcm_buffer;
    while (1) {
        if (xQueueReceive(pcm_queue, &pcm_buffer, portMAX_DELAY) == pdTRUE) {
            if (is_playing) {
                // 应用音量控制
                for (int i = 0; i < pcm_buffer.samples * 2; i++) {
                    pcm_buffer.pcm[i] = (pcm_buffer.pcm[i] * volume) / 100;
                }
                tud_audio_write(pcm_buffer.pcm, pcm_buffer.samples * 2);
            }
            free(pcm_buffer.pcm); // 释放动态分配的内存
        }
    }
}

void play_control_task(void *pvParameters)
{
    while (1) {
        // 模拟播放控制
        vTaskDelay(pdMS_TO_TICKS(1000));
        xSemaphoreTake(play_control_mutex, portMAX_DELAY);
        is_playing = !is_playing;
        xSemaphoreGive(play_control_mutex);
        ESP_LOGI(TAG, "Playback %s", is_playing ? "resumed" : "paused");
    }
}

void audio_app_main(void)
{
    // // 初始化SD卡
    // esp_err_t ret;
    // sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    // esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    //     .format_if_mount_failed = false,
    //     .max_files = 5,
    //     .allocation_unit_size = 16 * 1024
    // };
    // sdmmc_card_t *card;
    // ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
    //     return;
    // }

    // 初始化USB音频
    tusb_init();

    // 创建PCM数据队列
    pcm_queue = xQueueCreate(10, sizeof(pcm_buffer_t));
    if (pcm_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create PCM queue");
        return;
    }

    // 创建播放控制互斥量
    play_control_mutex = xSemaphoreCreateMutex();
    if (play_control_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create play control mutex");
        return;
    }

    // 创建USB音频任务
    xTaskCreate(usb_audio_task, "usb_audio_task", 4096, NULL, 5, NULL);

    // 创建播放控制任务
    xTaskCreate(play_control_task, "play_control_task", 2048, NULL, 4, NULL);

    // 打开MP3文件
    FILE *file = fopen("/:/test.mp3", "rb");
    if (file == NULL) {
        ESP_LOGE(TAG, "Failed to open MP3 file");
        return;
    }

    // 初始化MP3解码器
    MP3DecHandle mp3;
    MP3Dec_Init(&mp3);

    // 读取并解码MP3文件
    uint8_t buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        int16_t *pcm = malloc(PCM_BUFFER_SIZE * sizeof(int16_t));
        if (pcm == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for PCM buffer");
            break;
        }
        int samples = MP3Dec_Decode(&mp3, buffer, bytes_read, pcm, 0);
        if (samples > 0) {
            pcm_buffer_t pcm_buffer = { .pcm = pcm, .samples = samples };
            xQueueSend(pcm_queue, &pcm_buffer, portMAX_DELAY);
        } else {
            free(pcm);
        }
    }

    // 关闭文件
    fclose(file);

    // 卸载SD卡
    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(TAG, "SD card unmounted");
}


#endif