#include "UI.h"
#include <stdio.h>
#include "driver/i2c.h"
#include <string.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "heart_page"
#define MAX30105_ADDRESS 0x57 // 7-bit I2C Address
lv_timer_t* heart_rate_timer;
lv_obj_t *heart_page_chart;
lv_chart_series_t *ser;
lv_chart_series_t *ser2;
lv_chart_series_t *ser3;
lv_obj_t *ta;
lv_obj_t *label1;
extern lv_obj_t *main_screen;
MAX30105 particleSensor;
// 用于存储心跳时间戳的变量
int64_t lastBeatTime = 0;
float per_beatsPerMinute = 0.0;
float avg_beatsPerMinute = 0.0;
// 滑动窗口：存储最近 20 个心跳间隔时间
#define HEART_RATE_WINDOW_SIZE 20 // 每组数据的大小
float beatIntervals[HEART_RATE_WINDOW_SIZE];
uint8_t beatIntervalCount = 0; // 当前组中的数据数量
void heart_page_back_bnt(lv_event_t *e);
void MAX30105_task(lv_timer_t *timer);
static SemaphoreHandle_t lvgl_mutex;

void lv_port_sem_init(void)
{
    lvgl_mutex = xSemaphoreCreateMutex();
}

void lv_port_sem_take(void)
{
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
}

void lv_port_sem_give(void)
{
    xSemaphoreGive(lvgl_mutex);
}

void calculateHeartRate(void *data)
{int64_t beatTime = *(int64_t*)data;
    if (lastBeatTime > 0)
    {
        // 计算心跳间隔时间（秒）
        float beatInterval = (beatTime - lastBeatTime) / 1000000.0;

        // 计算单次心率（BPM）
        per_beatsPerMinute = 60.0 / beatInterval;
        // 打印心率
        //printf("Heart Rate: %.2f BPM\n", per_beatsPerMinute);
        char str[50];
        sprintf(str, "Heart Rate: %.2f BPM\n", per_beatsPerMinute);
        lv_textarea_add_text(ta, str);
        // 将心跳间隔时间添加到当前组
        beatIntervals[beatIntervalCount] = beatInterval;
        beatIntervalCount++;

        // 如果当前组已满，计算平均心率
        if (beatIntervalCount == HEART_RATE_WINDOW_SIZE)
        {
            float sum = 0.0;
            for (uint8_t i = 0; i < HEART_RATE_WINDOW_SIZE; i++)
            {
                sum += beatIntervals[i];
            }
            float averageInterval = sum / HEART_RATE_WINDOW_SIZE;

            // 计算平均心率（BPM）
            avg_beatsPerMinute = 60.0 / averageInterval;

            // 打印心率
            // printf("====================================\n");
            // printf("Average Heart Rate: %.2f BPM\n", avg_beatsPerMinute);
            // printf("====================================\n");
            char avg[50];
            sprintf(avg, "Average Rate: %.2f BPM\n", avg_beatsPerMinute);
            lv_textarea_set_text(ta, avg);
            lv_label_set_text(label1, avg);

            // 清空当前组，重新开始统计下一组
            beatIntervalCount = 0;
        }
    }

    // 更新上一次心跳时间
    lastBeatTime = beatTime;
}

void create_heart_page()
{
    lv_port_sem_init();

    lv_obj_t *heart_page = lv_obj_create(NULL);

    lv_obj_t *btn = lv_btn_create(heart_page);
    lv_obj_set_size(btn, LV_PCT(100), 20);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *img = lv_img_create(btn);
    lv_img_set_src(img, LV_SYMBOL_NEW_LINE);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "BACK");
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(btn, heart_page_back_bnt, LV_EVENT_CLICKED, NULL);

    ta = lv_textarea_create(heart_page);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 30);
    lv_textarea_set_placeholder_text(ta, "log");
    lv_obj_set_size(ta, LV_PCT(90), LV_PCT(20));

    label1 = lv_label_create(heart_page);
    lv_obj_align_to(label1, ta, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    heart_page_chart = lv_chart_create(heart_page);
    lv_obj_set_size(heart_page_chart, LV_PCT(90), LV_PCT(50));
    lv_obj_align_to(heart_page_chart, label1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
    lv_chart_set_range(heart_page_chart, LV_CHART_AXIS_PRIMARY_Y, -100, 100);
    lv_chart_set_update_mode(heart_page_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(heart_page_chart, 60);
    lv_obj_set_style_size(heart_page_chart, 0, LV_PART_INDICATOR);

    ser = lv_chart_add_series(heart_page_chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    ser2 = lv_chart_add_series(heart_page_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    ser3 = lv_chart_add_series(heart_page_chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    //xTaskCreatePinnedToCore(MAX30105_task, "MAX30105_task", 1024 * 3, NULL, 1, &MAX30105_task_handel, 1);
    lv_textarea_add_text(ta, "INIT MAX30105\n");
    lv_textarea_add_text(ta, "Done!");

    // 初始化 I2C 和传感器
    if (!particleSensor.begin(I2C_NUM_0, MAX30105_ADDRESS))
    {
        ESP_LOGE(TAG, "Failed to initialize MAX30105 sensor");
        return;
    }


    // 配置传感器
    uint8_t ledBrightness = 0x1F; // Options: 0=Off to 255=50mA
    uint8_t sampleAverage = 4;    // Options: 1, 2, 4, 8, 16, 32
    uint8_t ledMode = 2;          // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
    int sampleRate = 400;      // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    int pulseWidth = 411;      // Options: 69, 118, 215, 411
    int adcRange = 4096;       // Options: 2048, 4096, 8192, 16384
    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
    ESP_LOGI(TAG, "MAX30105 sensor configured successfully");
    // Enable FIFO rollover and data ready interrupt
    particleSensor.enableFIFORollover();
    particleSensor.enableDATARDY();
    particleSensor.clearFIFO();

    // Wake up the sensor (if it was in sleep mode)
    particleSensor.wakeUp();

    heart_rate_timer=lv_timer_create(MAX30105_task, 30, NULL);
    lv_timer_set_repeat_count(heart_rate_timer, -1);

    lv_disp_load_scr(heart_page);
}

void update_chart_ser_async(void *data) {
    lv_chart_set_next_value(heart_page_chart, ser, *(uint32_t *)data);
}
void update_chart_ser2_async(void *data) {
    lv_chart_set_next_value(heart_page_chart, ser2, *(int16_t *)data);
}
void MAX30105_task(lv_timer_t *timer)
{
        // Check if new data is available
        if (particleSensor.safeCheck(100)) {
            // Read IR value
            uint32_t irSignal = particleSensor.getIR();
            uint32_t ir_Signal=irSignal/1500;
            int16_t IR_AC_Signal =get_IR_AC_Signal_Current()/2;
            // lv_async_call(update_chart_ser_async, &ir_Signal);
            // lv_async_call(update_chart_ser2_async, &IR_AC_Signal);
            lv_chart_set_next_value(heart_page_chart, ser,ir_Signal);
            lv_chart_set_next_value(heart_page_chart, ser2,IR_AC_Signal);

            // lv_obj_invalidate(heart_page_chart);

            // Detect heartbeat
            bool beatDetected = checkForBeat(irSignal);
            if (beatDetected)
            {
                // 获取当前时间（微秒）
                int64_t beatTime = esp_timer_get_time();

                // 计算心率
                lv_async_call(calculateHeartRate,&beatTime);
            }
        }
        else
        {
            printf("No new data available\n");
        } 
}

void heart_page_back_bnt(lv_event_t *e)
{
    lv_obj_t *old_page = lv_scr_act();
    particleSensor.shutDown();
    lv_timer_del(heart_rate_timer);
    lv_disp_load_scr(main_screen);
    lv_obj_clean(old_page);
    lv_obj_del(old_page); 
}
