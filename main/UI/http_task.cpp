#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "jsmn.h"
#include "UI.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "WEATHER_APP"

// 从 IP地址查询接口 获取城市信息的 URL
#define IP_API_URL "http://qifu-api.baidubce.com/ip/local/geo/v1/district"

// 从 seniverse.com 获取天气信息的 URL 模板
#define WEATHER_API_URL "http://api.seniverse.com/v3/weather/now.json?key=SCmb3d-dfjAaPvroO&location=%s&language=zh-Hans&unit=c"

extern const char baidu_com_pem_start[] asm("_binary_baidu_com_pem_start");
extern const char baidu_com_pem_end[] asm("_binary_baidu_com_pem_end");

// 用于存储城市名称、天气信息和温度的全局变量
static char city[32] = {0};
static char weather_text[16] = {0};
static char temperature[8] = {0};

// 全局变量，用于存储 HTTP 响应数据
static char *output_buffer = NULL;
static int output_len = 0;

// 任务完成标志
static bool task_complete = false;

// URL 编码函数
void url_encode(const char *str, char *output, size_t output_len) {
    const char *hex = "0123456789ABCDEF";
    size_t len = strlen(str);
    size_t j = 0;

    for (size_t i = 0; i < len; i++) {
        if ((str[i] >= '0' && str[i] <= '9') ||
            (str[i] >= 'A' && str[i] <= 'Z') ||
            (str[i] >= 'a' && str[i] <= 'z') ||
            str[i] == '-' || str[i] == '_' || str[i] == '.' || str[i] == '~') {
            // 不需要编码的字符
            if (j + 1 >= output_len) break;
            output[j++] = str[i];
        } else {
            // 需要编码的字符
            if (j + 3 >= output_len) break;
            output[j++] = '%';
            output[j++] = hex[(str[i] >> 4) & 0xF];
            output[j++] = hex[str[i] & 0xF];
        }
    }
    output[j] = '\0';  // 确保字符串以 null 结尾
}

// HTTP 事件处理函数
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // 如果不是分块传输编码，按原方式处理
                if (output_buffer == NULL) {
                    output_buffer = (char *)malloc(esp_http_client_get_content_length(evt->client) + 1);
                    output_len = 0;
                    if (output_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
            } else {
                // 如果是分块传输编码，动态扩展缓冲区
                output_buffer = (char *)realloc(output_buffer, output_len + evt->data_len + 1);
                if (output_buffer == NULL) {
                    ESP_LOGE(TAG, "Failed to reallocate memory for output buffer");
                    return ESP_FAIL;
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
            }
            output_buffer[output_len] = '\0';  // 确保字符串以 null 结尾
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                ESP_LOGI(TAG, "Response: %s", output_buffer);
            }
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;

        default:
            break;
    }
    return ESP_OK;
}

// 使用 jsmn 解析 JSON 数据
int parse_json(const char *json_string, const char *key, char *value, size_t value_len)
{
    if (json_string == NULL || key == NULL || value == NULL || value_len == 0) {
        ESP_LOGE(TAG, "Invalid input parameters");
        return -1;
    }

    jsmn_parser parser;
    jsmn_init(&parser);

    // 解析 JSON 字符串
    int num_tokens = jsmn_parse(&parser, json_string, strlen(json_string), NULL, 0);
    if (num_tokens < 0) {
        ESP_LOGE(TAG, "Failed to parse JSON: %d", num_tokens);
        return -1;
    }

    // 分配 tokens
    jsmntok_t *tokens = (jsmntok_t *)malloc(sizeof(jsmntok_t) * num_tokens);
    if (tokens == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for tokens");
        return -1;
    }

    jsmn_init(&parser);
    num_tokens = jsmn_parse(&parser, json_string, strlen(json_string), tokens, num_tokens);

    // 遍历 tokens 查找 key
    for (int i = 0; i < num_tokens; i++) {
        if (tokens[i].type == JSMN_STRING &&
            strncmp(json_string + tokens[i].start, key, tokens[i].end - tokens[i].start) == 0) {
            // 找到 key，下一个 token 是 value
            if (i + 1 < num_tokens && tokens[i + 1].type == JSMN_STRING) {
                int len = tokens[i + 1].end - tokens[i + 1].start;
                len = len < value_len - 1 ? len : value_len - 1;
                strncpy(value, json_string + tokens[i + 1].start, len);
                value[len] = '\0';
                free(tokens);
                return 0;
            }
        }
    }

    free(tokens);
    return -1;
}

// 获取当前城市
void get_current_city() {
    esp_http_client_config_t config = {
        .url = IP_API_URL,
        .cert_pem = baidu_com_pem_start, // 使用自定义证书
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        // 解析 JSON 获取城市名称
        if (output_buffer != NULL && strlen(output_buffer) > 0) {
            if (parse_json(output_buffer, "city", city, sizeof(city)) == 0) {
                ESP_LOGI(TAG, "Current city: %s", city);
            } else {
                ESP_LOGE(TAG, "Failed to parse city from JSON");
            }
        } else {
            ESP_LOGE(TAG, "Invalid or empty response data");
        }

        // 释放缓冲区
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

// 从 seniverse.com 获取当前城市的天气
void get_weather_for_city(const char *city)
{
    char weather_url[256];
    char encoded_city[128];

    // 对城市名称进行 URL 编码
    url_encode(city, encoded_city, sizeof(encoded_city));
    snprintf(weather_url, sizeof(weather_url), WEATHER_API_URL, encoded_city);

    esp_http_client_config_t config = {
        .url = weather_url,
        .cert_pem = NULL, // 使用内置 CA 证书
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %"PRIu64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));

        // 读取响应数据
        if (output_buffer != NULL && strlen(output_buffer) > 0) {
            // 解析 JSON 获取天气信息
            if (parse_json(output_buffer, "text", weather_text, sizeof(weather_text)) == 0 &&
                parse_json(output_buffer, "temperature", temperature, sizeof(temperature)) == 0) {
                ESP_LOGI(TAG, "Weather in %s: %s, Temperature: %s°C", city, weather_text, temperature);
            } else {
                ESP_LOGE(TAG, "Failed to parse weather from JSON");
            }
        } else {
            ESP_LOGE(TAG, "Invalid or empty response data");
        }

        // 释放缓冲区
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

// 主任务
void weather_task(void *pvParameters)
{
    // 获取当前城市
    get_current_city();

    if (strlen(city) > 0) {
        // 获取当前城市的天气
        get_weather_for_city(city);
    } else {
        ESP_LOGE(TAG, "Failed to get current city");
    }

    // 任务完成
    task_complete = true;
    vTaskDelete(NULL);
}

// 调用 htpp_task() 返回 city, weather_text, temperature
void htpp_weather(char *out_city, char *out_weather_text, char *out_temperature)
{
    ESP_LOGI(TAG, "htpp_weather task start");
    // 初始化网络
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建天气任务
    xTaskCreate(&weather_task, "weather_task", 8192, NULL, 5, NULL);

    // 等待任务完成
    while (!task_complete) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    // 返回结果
    strncpy(out_city, city, 32);
    strncpy(out_weather_text, weather_text, 16);
    strncpy(out_temperature, temperature, 8);

    ESP_LOGI(TAG, "htpp_weather task finished");
}