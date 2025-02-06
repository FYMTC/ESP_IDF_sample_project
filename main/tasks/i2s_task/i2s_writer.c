#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "esp_system.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_audio_dec.h"
static const char *TAG = "i2s_writer";
static const char err_reason[][30] = {"input param is invalid",
                                      "operation timeout"};
static i2s_chan_handle_t tx_handle = NULL;
static i2s_chan_handle_t rx_handle = NULL;

// 定义I2S引脚
#define I2S_DSDIN GPIO_NUM_40
#define I2S_SCLK GPIO_NUM_45
#define I2S_LRCK GPIO_NUM_9
#define I2S_MCLK GPIO_NUM_0

#define EXAMPLE_SAMPLE_RATE     (44100)
#define EXAMPLE_MCLK_MULTIPLE   (256) // If not using 24-bit data width, 256 should be enough
#define EXAMPLE_MCLK_FREQ_HZ    (EXAMPLE_SAMPLE_RATE * EXAMPLE_MCLK_MULTIPLE)
#define I2S_NUM I2S_NUM_0

//音频数据队列
QueueHandle_t i2s_audio_data_queue;

esp_err_t pa_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((1ULL<<38) | (1ULL<<39) | (1ULL<<42));
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    gpio_set_level(38, 0);
    gpio_set_level(39, 0);
    gpio_set_level(42, 1);
    return ESP_OK;
}

// 定义一个静态函数 i2s_driver_init，用于初始化 I2S 驱动
static esp_err_t i2s_driver_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    // 设置 chan_cfg 的 auto_clear 属性为 true，表示自动清除 DMA 缓冲区中的旧数据
    chan_cfg.auto_clear = true; 
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, &rx_handle));
    i2s_std_config_t std_cfg = {
        // 配置时钟，参数为采样率
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(EXAMPLE_SAMPLE_RATE),
        // 参数包括数据位宽和插槽模式
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK,  // 主时钟引脚
            .bclk = I2S_SCLK,  // 串行时钟引脚
            .ws = I2S_LRCK,    // 左右声道时钟引脚
            .dout = I2S_DSDIN, // 数据输出引脚
            .din = -1,         // 数据输入引脚（不使用）
            .invert_flags = {
                .mclk_inv = false, // 主时钟引脚不反转
                .bclk_inv = false, // 串行时钟引脚不反转
                .ws_inv = false,   // 左右声道时钟引脚不反转
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = EXAMPLE_MCLK_MULTIPLE;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
    return ESP_OK;
}

static void i2s_music(void *args)
{
    esp_err_t ret = ESP_OK;
    size_t bytes_write = 0;
    esp_audio_dec_out_frame_t out_frame;

    while (1)
    {
        // 从队列中接收音频数据
        if (xQueueReceive(i2s_audio_data_queue, &out_frame, portMAX_DELAY) == pdTRUE)
        {
            // 将接收到的音频数据写入I2S
            ret = i2s_channel_write(tx_handle, out_frame.buffer, out_frame.decoded_size, &bytes_write, portMAX_DELAY);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "[music] i2s write failed, %s", err_reason[ret == ESP_ERR_TIMEOUT]);
                abort();
            }
        }
        else
        {
            ESP_LOGE(TAG, "[music] Failed to receive audio data from queue.");
            abort();
        }
    }
    vTaskDelete(NULL);
}

void I2S_writer_init(void)
{
    printf("i2s codec example start\n-----------------------------\n");

    // 初始化音频数据队列
    i2s_audio_data_queue = xQueueCreate(10, sizeof(esp_audio_dec_out_frame_t));
    if (i2s_audio_data_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create audio data queue");
        abort();
    }

    /* Initialize i2s peripheral */
    if (i2s_driver_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "i2s driver init failed");
        abort();
    }
    else
    {
        ESP_LOGI(TAG, "i2s driver init success");
    }

    /* Initialize i2c peripheral*/
    if (pa_init() != ESP_OK) {
        ESP_LOGE(TAG, "PA & codec init failed");
        abort();
    } else {
        ESP_LOGI(TAG, "PA & codec init success");
    }

    /* Play a piece of music in music mode */
    xTaskCreate(i2s_music, "i2s_music_player", 4096, NULL, 5, NULL);
}