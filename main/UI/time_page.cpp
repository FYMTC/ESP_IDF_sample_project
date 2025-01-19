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
#include "jsmn.h"
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

struct tm timeinfo;
bool timeNeedsUpdate = false;
bool timeLogUpdate = false;

const char *timeAPI = "http://worldtimeapi.org/api/timezone/Asia/Shanghai";
float battery_voltage = 3.7; // 示例电池电压

// NVS 键名
static const char *NVS_NAMESPACE = "time_storage";
static const char *NVS_TIME_KEY = "last_saved_time";

// 函数声明
void getTimeFromAPI();
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
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP is already initialized, skipping re-initialization");
        return;
    }

    ESP_LOGI(TAG, "Initializing SNTP");

    // 设置时区为 CST-8（中国标准时间，UTC+8）
    setenv("TZ", "CST-8", 1); // CST-8 表示东八区
    tzset();                  // 使时区设置生效
    
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    sntp_set_sync_interval(1000*60); // 设置同步间隔为 60 秒
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

// // 使用 jsmn 解析 JSON
// int parse_json(const char *json_string, size_t length)
// {
//     jsmn_parser parser;
//     jsmn_init(&parser);

//     // 假设最多 128 个 token
//     jsmntok_t tokens[128];
//     int num_tokens = jsmn_parse(&parser, json_string, length, tokens, 128);

//     if (num_tokens < 0)
//     {
//         ESP_LOGE(TAG, "Failed to parse JSON: %d", num_tokens);
//         return -1;
//     }

//     // 遍历 tokens
//     for (int i = 0; i < num_tokens; i++)
//     {
//         jsmntok_t *token = &tokens[i];

//         if (token->type == JSMN_STRING && token->size == 1)
//         {
//             // 检查键名
//             char key[32];
//             snprintf(key, sizeof(key), "%.*s", token->end - token->start, json_string + token->start);

//             if (strcmp(key, "datetime") == 0)
//             {
//                 // 获取 datetime 值
//                 jsmntok_t *value_token = &tokens[i + 1];
//                 char datetime[64];
//                 snprintf(datetime, sizeof(datetime), "%.*s", value_token->end - value_token->start, json_string + value_token->start);
//                 ESP_LOGI(TAG, "Datetime: %s", datetime);
//             }
//             else if (strcmp(key, "unixtime") == 0)
//             {
//                 // 获取 unixtime 值
//                 jsmntok_t *value_token = &tokens[i + 1];
//                 char unixtime_str[16];
//                 snprintf(unixtime_str, sizeof(unixtime_str), "%.*s", value_token->end - value_token->start, json_string + value_token->start);
//                 unsigned long epochTime = strtoul(unixtime_str, NULL, 10);

//                 // 设置系统时间（东八区）
//                 struct timeval tv = {
//                     .tv_sec = epochTime + 8 * 60 * 60, // 东八区偏移
//                     .tv_usec = 0};
//                 settimeofday(&tv, NULL);     // 更新系统时间
//                 save_time_to_nvs(tv.tv_sec); // 保存时间到 NVS
//                 ESP_LOGI(TAG, "Epoch time: %lu", epochTime);
//             }
//         }
//     }

//     return 0;
// }

// // 获取网络时间（增加超时时间和备用时间源）
// void getTimeFromAPI()
// {
//     esp_http_client_config_t config = {
//         .url = timeAPI,
//         .timeout_ms = 1000, // 增加超时时间为 1 秒
//     };
//     esp_http_client_handle_t client = esp_http_client_init(&config);

//     int retry_count = 3; // 重试次数
//     esp_err_t err = ESP_FAIL;

//     while (retry_count > 0)
//     {
//         err = esp_http_client_perform(client);
//         if (err == ESP_OK)
//         {
//             int content_length = esp_http_client_get_content_length(client);
//             if (content_length <= 0)
//             {
//                 ESP_LOGE(TAG, "Invalid content length");
//                 break;
//             }

//             char *buffer = (char *)malloc(content_length + 1);
//             if (buffer == NULL)
//             {
//                 ESP_LOGE(TAG, "Failed to allocate memory for buffer");
//                 break;
//             }

//             int read_len = esp_http_client_read(client, buffer, content_length);
//             if (read_len <= 0)
//             {
//                 ESP_LOGE(TAG, "Failed to read HTTP response");
//                 free(buffer);
//                 break;
//             }
//             buffer[read_len] = '\0';

//             // 解析 JSON
//             if (parse_json(buffer, read_len) != 0)
//             {
//                 ESP_LOGE(TAG, "Failed to parse JSON");
//             }

//             free(buffer);
//             break; // 成功获取时间，退出重试循环
//         }
//         else
//         {
//             ESP_LOGE(TAG, "HTTP request failed (retries left: %d): %s", retry_count - 1, esp_err_to_name(err));
//             retry_count--;
//             vTaskDelay(pdMS_TO_TICKS(1000)); // 增加重试间隔为 1 秒
//         }
//     }

//     if (err != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Failed to get time from API after retries, falling back to SNTP");
//         // 使用 SNTP 作为备用时间源
//         initialize_sntp();
//     }

//     esp_http_client_cleanup(client);

//     // 获取当前时间并保存到 NVS
//     time_t now;
//     time(&now);
//     save_time_to_nvs(now); // 保存 UTC 时间到 NVS
// }

// 格式化毫秒数为 HH:MM:SS
const char *formatMillis(unsigned long millis)
{
    static char buffer[9];
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
    }
    if (time_page_fash_task_handle == NULL)
    {
        xTaskCreate(time_page_fash_task, "time_fash_task", 1024*3, NULL, 1, &time_page_fash_task_handle);
    }

    lv_scr_load_anim(time_page, LV_SCR_LOAD_ANIM_OVER_TOP, 100, 300, false);
}

// 时间更新任务
void time_page_fash_task(void *pvParameters)
{
    while (1)
    {
        unsigned long currentMillis = esp_timer_get_time() / 1000;
        // if (currentMillis % (60 * 1000) == 0)
        // {
        //     // 检查 WiFi 连接状态
        //     if (is_wifi_connected())
        //     {

        //     }
        //     else
        //     {

        //     }

            
        // }

        // 获取本地时间
        if (!getLocalTime(&timeinfo))
        {
            ESP_LOGE(TAG, "Failed to obtain time");
            vTaskDelay(pdMS_TO_TICKS(1000)); // 延迟 1 秒后重试
            continue;
        }

        // 格式化并显示时间
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        lv_label_set_text(label_second, strftime_buf);

        strftime(strftime_buf, sizeof(strftime_buf), "%H:%M", &timeinfo);
        lv_label_set_text(label_time, strftime_buf);

        lv_label_set_text(label_running, formatMillis(currentMillis));

        // 更新电池电压显示
        char buf[32];
        snprintf(buf, sizeof(buf), "Battery: %.2fV", battery_voltage);
        lv_label_set_text(label_BATTERY, buf);

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