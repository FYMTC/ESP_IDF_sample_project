#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "UI.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "nvs.h"

// 日志标签
static const char *TAG = "TIME PAGE";
extern lv_obj_t *main_screen;

// 全局变量
lv_obj_t *label_time;
lv_obj_t *label_second;
lv_obj_t *label_running;
lv_obj_t *label_BATTERY;
lv_obj_t *time_page;
lv_obj_t *label_weather;

struct tm timeinfo;
bool timeNeedsUpdate = false;
bool timeLogUpdate = false;

const char *timeAPI = "http://worldtimeapi.org/api/timezone/Asia/Shanghai";
float battery_voltage = 3.7; // 示例电池电压

// NVS 键名
static const char *NVS_NAMESPACE = "time_storage";
static const char *NVS_TIME_KEY = "last_saved_time";

// 函数声明

const char *formatMillis(unsigned long millis);
void time_page_fash_task(void *pvParameters);
TaskHandle_t time_page_fash_task_handle;
void time_page_cb(lv_event_t *event);
bool is_wifi_connected();
void save_time_to_nvs(time_t time);
time_t load_time_from_nvs();

// 检查 WiFi 连接状态
bool is_wifi_connected()
{
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

// 保存时间到 NVS
void save_time_to_nvs(time_t time)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_i64(nvs_handle, NVS_TIME_KEY, (int64_t)time);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save time to NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Time saved to NVS: %lld", (int64_t)time);
    }

    nvs_commit(nvs_handle); // 提交更改
    nvs_close(nvs_handle);
}

// 从 NVS 加载时间
time_t load_time_from_nvs()
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return 0;
    }

    int64_t saved_time = 0;
    err = nvs_get_i64(nvs_handle, NVS_TIME_KEY, &saved_time);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load time from NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "Time loaded from NVS: %lld", saved_time);
    }

    nvs_close(nvs_handle);
    return (time_t)saved_time;
}
// 时间同步完成后的回调函数
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized! Unix time: %lld", (long long)tv->tv_sec);
    // 获取当前时间并保存到 NVS
    time_t now;
    time(&now);
    save_time_to_nvs(now); // 保存 UTC 时间到 NVS
    ESP_LOGI(TAG, "Time saved to NVS: %lld", (int64_t)now);
}
// 初始化 SNTP 并设置时区
void initialize_sntp(void)
{
    // 检查 SNTP 是否已经初始化
    if (esp_sntp_enabled())
    {
        ESP_LOGI(TAG, "SNTP is already initialized, skipping re-initialization");
        return;
    }

    ESP_LOGI(TAG, "Initializing SNTP");

    // 设置时区为 CST-8（中国标准时间，UTC+8）
    setenv("TZ", "CST-8", 1); // CST-8 表示东八区
    tzset();                  // 使时区设置生效

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    sntp_set_sync_interval(1000 * 60); // 设置同步间隔为 60 秒
    // 注册时间同步回调函数
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    ESP_LOGI(TAG, "SNTP initialized and started");
}
// 获取本地时间
bool getLocalTime(struct tm *info)
{
    time_t now;
    time(&now);
    localtime_r(&now, info); // 转换为本地时间（考虑时区）
    return true;
}
// 格式化毫秒数为 HH:MM:SS
const char *formatMillis(unsigned long millis)
{
    static char buffer[12];
    unsigned long seconds = millis / 1000;
    unsigned long hours = seconds / 3600;
    seconds %= 3600;
    unsigned long minutes = seconds / 60;
    seconds %= 60;
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    return buffer;
}

// 创建时间页面
void create_time_page()
{

    // 检查 WiFi 连接状态
    if (!is_wifi_connected())
    {
        ESP_LOGW(TAG, "WiFi not connected");
    }
    else
    {
        // 初始化 SNTP
        initialize_sntp();
        // getTimeFromAPI();
    }

    if (time_page == NULL)
    {
        ESP_LOGW(TAG, "create_time_page loading time from NVS");
        time_t saved_time = load_time_from_nvs();
        if (saved_time > 0)
        {
            struct timeval tv = {
                .tv_sec = saved_time,
                .tv_usec = 0};
            settimeofday(&tv, NULL); // 设置系统时间为 NVS 中保存的时间
        }

        time_page = lv_obj_create(NULL);

        lv_obj_add_event_cb(time_page, time_page_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_set_style_bg_color(time_page, lv_color_black(), 0);

        label_time = lv_label_create(time_page);
        lv_label_set_recolor(label_time, true);
        lv_label_set_text(label_time, "#ffffff 00:00#");
        lv_obj_set_style_text_font(label_time, &my_font, 0);
        lv_obj_center(label_time);
        lv_obj_set_style_text_color(label_time, lv_color_white(), 0);

        label_second = lv_label_create(time_page);
        lv_label_set_recolor(label_second, true);
        lv_label_set_text(label_second, "#ffffff 00-00-00 00:00:00#");
        lv_obj_set_style_text_color(label_second, lv_color_white(), 0);
        lv_obj_align_to(label_second, label_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);

        label_BATTERY = lv_label_create(time_page);
        char buf[32];
        snprintf(buf, sizeof(buf), "Battery: %.2fV", battery_voltage); // 显示电池电压，保留两位小数
        lv_label_set_text(label_BATTERY, buf);
        lv_obj_align_to(label_BATTERY, label_second, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

        label_running = lv_label_create(time_page);
        lv_label_set_text(label_running, formatMillis(esp_timer_get_time() / 1000));
        lv_obj_align_to(label_running, label_BATTERY, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

        label_weather = lv_label_create(time_page);
        lv_label_set_text(label_weather, "");
        lv_obj_set_style_text_font(label_weather, &NotoSansSC_Medium_3500, 0);
        lv_obj_align(label_weather, LV_ALIGN_TOP_LEFT, 0, 0);
        // // 更新天气
        // char city[32] = {0};         // 存储城市名称
        // char weather_text[16] = {0}; // 存储天气描述
        // char temperature[8] = {0};   // 存储温度值

        // htpp_weather(city, weather_text, temperature);
        // snprintf(buf, sizeof(buf), "%s %s %s°C", city, weather_text, temperature);
        // lv_label_set_text(label_weather, buf);
    }
    if (time_page_fash_task_handle == NULL)
    {
        xTaskCreate(time_page_fash_task, "time_fash_task", 1024 * 3, NULL, 1, &time_page_fash_task_handle);
    }

    lv_scr_load_anim(time_page, LV_SCR_LOAD_ANIM_OVER_TOP, 100, 300, false);
}

// 时间更新任务
void time_page_fash_task(void *pvParameters)
{
    static u8_t time_sync = 0;
    while (1)
    {
        char buf[64];
        
        unsigned long currentMillis = esp_timer_get_time() / 1000;//microseconds in esp_timer_get_time

        if (time_sync % (60) == 0)
        {
            // 检查 WiFi 连接状态
            if (is_wifi_connected())
            {
                // 更新天气
                char city[32] = {0};         // 存储城市名称
                char weather_text[16] = {0}; // 存储天气描述
                char temperature[8] = {0};   // 存储温度值

                htpp_weather(city, weather_text, temperature);
                snprintf(buf, sizeof(buf), "%s %s %s°C", city, weather_text, temperature);
                lv_label_set_text(label_weather, buf);
            }
            else
            {
                ESP_LOGI(TAG, "WiFi not connected, skipping weather update");
            }
        }

        // 获取本地时间
        if (!getLocalTime(&timeinfo))
        {
            ESP_LOGE(TAG, "Failed to obtain time");
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延迟 1 秒后重试
            continue;
        }

        // 格式化并显示时间

        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        lv_label_set_text(label_second, buf);

        strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
        lv_label_set_text(label_time, buf);

        lv_label_set_text(label_running, formatMillis(currentMillis));

        // 更新电池电压显示
        snprintf(buf, sizeof(buf), "Battery: %.2fV", battery_voltage);
        lv_label_set_text(label_BATTERY, buf);

        time_sync++;
        vTaskDelay(pdMS_TO_TICKS(1000)); // 每秒刷新一次时间
    }
}

// 页面事件回调
void time_page_cb(lv_event_t *event)
{
    ESP_LOGI(TAG, "Page event: switching to main_screen");
    vTaskDelete(time_page_fash_task_handle);
    time_page_fash_task_handle = NULL;
    lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_OVER_TOP, 100, 300, false);
}