#include "wifi_service.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/queue.h"
#include <string.h>    // 包含 memcpy 的声明
#include "sdkconfig.h" // 包含SDK配置

// 默认的 SSID 和密码
const char *default_ssid = "404";
const char *default_password = "abcd0404";

static const char *TAG = "wifi_service";
bool complete_wifi_scan_flag = false;
static wifi_scan_result_t scan_results[MAX_SCAN_RESULTS];                                // 保存扫描结果
static uint8_t scan_result_count = 0;                                                    // 当前扫描结果数量
static QueueHandle_t wifi_connect_queue = NULL;                                          // 用于接收连接请求的队列
static bool is_wifi_enabled = false;                                                     // Wi-Fi 是否启用
static bool is_scan_enabled = true;                                                      // Wi-Fi 扫描是否启用
static wifi_connection_status_t connection_status = WIFI_CONNECTION_STATUS_DISCONNECTED; // 当前连接状态
static bool is_scanning = false;                                                         // 当前是否正在扫描

TaskHandle_t wifi_scan_task_handle = NULL;    // 扫描任务句柄
TaskHandle_t wifi_connect_task_handle = NULL; // 连接任务句柄

/*fast scan*/
// 默认的扫描方法（快速扫描或全信道扫描）
#if 0
#define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#else
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#endif /*CONFIG_EXAMPLE_SCAN_METHOD*/

// 排序方法（按信号强度或安全性）
#if 1
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#else
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#endif /*CONFIG_EXAMPLE_SORT_METHOD*/

// 信号强度和认证模式的阈值
#define DEFAULT_RSSI -127
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN // WIFI_AUTH_OPEN WIFI_AUTH_WEP WIFI_AUTH_WPA_PSK WIFI_AUTH_WPA2_PSK
/*power save*/
#define DEFAULT_LISTEN_INTERVAL 3 // 定义默认监听间隔
#define DEFAULT_BEACON_TIMEOUT 6  // 定义默认信标超时

// 保存 Wi-Fi 配置到 NVS
static void save_wifi_service_config_to_nvs(const char *ssid, const char *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save SSID: %s", esp_err_to_name(err));
    }

    err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save password: %s", esp_err_to_name(err));
    }

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

// 从 NVS 加载 Wi-Fi 配置
static bool load_wifi_config_from_nvs(uint8_t *ssid, uint8_t *password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_WIFI_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return false;
    }

    size_t ssid_len = 33;
    size_t password_len = 65;
    err = nvs_get_str(nvs_handle, "ssid", (char *)ssid, &ssid_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load SSID: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    err = nvs_get_str(nvs_handle, "password", (char *)password, &password_len);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load password: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }

    nvs_close(nvs_handle);
    return true;
}

// 获取当前连接的 Wi-Fi 配置
static bool get_current_wifi_config(wifi_config_t *config)
{
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get current AP info: %s", esp_err_to_name(err));
        return false;
    }
    strncpy((char *)config->sta.ssid, (char *)ap_info.ssid, 32);
    config->sta.ssid[32] = '\0';

    // 当前连接的 Wi-Fi 密码无法直接获取，只能从 NVS 中加载
    if (!load_wifi_config_from_nvs(config->sta.ssid, config->sta.password))
    {
        ESP_LOGE(TAG, "Failed to load password for current SSID");
        return false;
    }
    return true;
}

// 事件处理函数
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) // 扫描开始
    {
        connection_status = WIFI_CONNECTION_STATUS_CONNECTING;
        ESP_LOGI(TAG, "Wi-Fi connecting");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) // 连接断开
    {
        connection_status = WIFI_CONNECTION_STATUS_FAILED;
        ESP_LOGI(TAG, "Wi-Fi disconnected, restarting scan");

        // 重试连接
        esp_wifi_connect();

        // // 重新启动扫描任务
        // if (is_scan_enabled && is_wifi_enabled)
        // {
        //     xTaskNotifyGive(wifi_scan_task_handle); // 唤醒扫描任务
        // }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) // 扫描完成
    {
        complete_wifi_scan_flag = true;
        is_scanning = false; // 标记扫描完成
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) // 连接成功
    {
        connection_status = WIFI_CONNECTION_STATUS_CONNECTED;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected to AP, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        complete_wifi_scan_flag = true;
    }
}

// 扫描任务
static void wifi_scan_task(void *pvParameters)
{
    while (1)
    {
        if (wifi_scan_task_handle == NULL)
        {
            break; // 如果任务句柄无效，退出任务
        }
        uint16_t ap_count = 0;
        if (is_scan_enabled && is_wifi_enabled)
        {
            is_scanning = true; // 标记正在扫描

            wifi_scan_config_t scan_config = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false};

            // 启动扫描
            esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
                is_scanning = false; // 标记扫描失败
                continue;
            }
            ESP_LOGI(TAG, "Scan START");

            esp_wifi_scan_get_ap_num(&ap_count);

            wifi_ap_record_t ap_records[MAX_SCAN_RESULTS];
            esp_wifi_scan_get_ap_records(&ap_count, ap_records);

            scan_result_count = (ap_count > MAX_SCAN_RESULTS) ? MAX_SCAN_RESULTS : ap_count;
            for (int i = 0; i < scan_result_count; i++)
            {
                strncpy(scan_results[i].ssid, (char *)ap_records[i].ssid, 32);
                scan_results[i].ssid[32] = '\0';
                // scan_results[i].rssi = ap_records[i].rssi;
                // scan_results[i].auth_mode = ap_records[i].authmode;
            }

            ESP_LOGI(TAG, "Scan completed, found %d APs", scan_result_count);
        }
        if (ap_count == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
        }
    }
    wifi_scan_task_handle = NULL;
    vTaskDelete(NULL);
}

/// 连接任务
static void wifi_connect_task(void *pvParameters)
{
    while (1)
    {
        if (wifi_connect_task_handle == NULL)
        {
            break; // 如果任务句柄无效，退出任务
        }
        char ssid[33] = {0};
        char password[65] = {0};
        if (xQueueReceive(wifi_connect_queue, &ssid, portMAX_DELAY))
        {
            xQueueReceive(wifi_connect_queue, &password, portMAX_DELAY);

            // 检查 Wi-Fi 是否已启动
            if (!is_wifi_enabled)
            {
                ESP_LOGE(TAG, "Wi-Fi is not enabled, cannot connect");
                continue;
            }
            esp_wifi_disconnect();

            // 配置Wi-Fi连接参数
            wifi_config_t wifi_config = {
                .sta = {
                    .ssid = "",
                    .password = "",
                    .scan_method = DEFAULT_SCAN_METHOD,
                    .sort_method = DEFAULT_SORT_METHOD,
                    .threshold.rssi = DEFAULT_RSSI,
                    .threshold.authmode = DEFAULT_AUTHMODE,
                },
            };
            strncpy((char *)wifi_config.sta.ssid, ssid, 32);
            strncpy((char *)wifi_config.sta.password, password, 64);

            ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);
            ESP_LOGI(TAG, "Wi-Fi config set: SSID=%s, Password=%s", wifi_config.sta.ssid, wifi_config.sta.password);

            // 设置 Wi-Fi 配置
            esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to set Wi-Fi config: %s", esp_err_to_name(ret));
                continue;
            }

            // 连接 Wi-Fi
            ret = esp_wifi_connect();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to connect to Wi-Fi: %s", esp_err_to_name(ret));
                continue;
            }

            ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

            // 保存 Wi-Fi 配置到 NVS
            save_wifi_service_config_to_nvs(ssid, password);
        }
    }
    // 任务退出时清理
    wifi_connect_task_handle = NULL;
    vTaskDelete(NULL);
}
// 重新扫描获取指定 SSID 的详细信息
static bool get_ap_details(uint8_t *ssid, wifi_ap_record_t *ap_info)
{
    wifi_scan_config_t scan_config = {
        .ssid = ssid, // 指定 SSID
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true};

    // 启动扫描
    esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start scan: %s", esp_err_to_name(ret));
        return false;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    wifi_ap_record_t ap_records[1]; // 只需要一个结果
    esp_wifi_scan_get_ap_records(&ap_count, ap_records);

    if (ap_count > 0)
    {
        memcpy(ap_info, &ap_records[0], sizeof(wifi_ap_record_t));
        return true;
    }

    return false;
}
// WiFi 服务初始化
void wifi_service_init(void)
{
    // 从 NVS 读取 Wi-Fi 开关状态
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return;
    }
    int8_t wifi_enabled = 1; // 默认启用 Wi-Fi
    ret = nvs_get_i8(nvs_handle, NVS_KEY_WIFI_ENABLED, &wifi_enabled);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Failed to read Wi-Fi enabled state: %s", esp_err_to_name(ret));
    }
    nvs_close(nvs_handle);
    // 设置 Wi-Fi 开关状态
    is_wifi_enabled = (wifi_enabled == 1);

    // 初始化 Wi-Fi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    wifi_config_t wifi_config = {
        .sta = {
            .listen_interval = DEFAULT_LISTEN_INTERVAL,
        },
    };
    // 从 NVS 中加载 Wi-Fi 配置
    if (!load_wifi_config_from_nvs(wifi_config.sta.ssid, wifi_config.sta.password)) {
        // 如果加载失败，使用默认的 SSID 和密码
        ESP_LOGW(TAG, "Failed to load Wi-Fi config from NVS, using default SSID and password");
        strncpy((char *)wifi_config.sta.ssid, default_ssid, 32);
        strncpy((char *)wifi_config.sta.password, default_password, 64);
    }


    // 启动 Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 创建连接请求队列
    wifi_connect_queue = xQueueCreate(2, sizeof(char[33])); // 队列大小为 1
    if (wifi_connect_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create wifi_connect_queue");
        return;
    }
    // 如果 Wi-Fi 启用，启动 Wi-Fi
    if (is_wifi_enabled)
    {
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_inactive_time(WIFI_IF_STA, DEFAULT_BEACON_TIMEOUT));
    }
}

// 获取最近的扫描结果
uint8_t wifi_service_get_scan_results(wifi_scan_result_t *results, uint8_t max_results)
{
    uint8_t count = (scan_result_count > max_results) ? max_results : scan_result_count;
    memcpy(results, scan_results, count * sizeof(wifi_scan_result_t)); // 使用 memcpy 复制数据
    return count;
}

// 请求连接到指定 SSID
bool wifi_service_connect(const char *ssid, const char *password)
{
    if (wifi_connect_queue == NULL)
    {
        ESP_LOGE(TAG, "wifi_connect_queue is not initialized");
        return false;
    }

    // 检查 Wi-Fi 是否已启动
    if (!is_wifi_enabled)
    {
        ESP_LOGE(TAG, "Wi-Fi is not enabled, cannot connect");
        return false;
    }

    // 检查队列是否已满
    if (uxQueueSpacesAvailable(wifi_connect_queue) == 0)
    {
        ESP_LOGE(TAG, "wifi_connect_queue is full");
        return false;
    }

    // 发送 SSID
    if (xQueueSend(wifi_connect_queue, ssid, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to send SSID to queue");
        return false;
    }

    // 发送密码
    if (xQueueSend(wifi_connect_queue, password, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to send password to queue");
        return false;
    }

    return true;
}
// 打开或关闭 Wi-Fi
void wifi_service_set_wifi_enabled(bool enabled)
{
    if (is_wifi_enabled == enabled)
    {
        return; // 状态未变化，直接返回
    }

    // 更新 Wi-Fi 开关状态
    is_wifi_enabled = enabled;

    // 保存 Wi-Fi 开关状态到 NVS
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_WIFI_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return;
    }

    ret = nvs_set_i8(nvs_handle, NVS_KEY_WIFI_ENABLED, enabled ? 1 : 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save Wi-Fi enabled state: %s", esp_err_to_name(ret));
    }

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    // 启动或停止 Wi-Fi
    if (enabled)
    {
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_ERROR_CHECK(esp_wifi_set_inactive_time(WIFI_IF_STA, DEFAULT_BEACON_TIMEOUT));
    }
    else
    {
        ESP_ERROR_CHECK(esp_wifi_stop());
    }

    ESP_LOGI(TAG, "Wi-Fi %s", enabled ? "enabled" : "disabled");
}

// 打开或关闭 Wi-Fi 扫描
void wifi_service_set_scan_enabled(bool enabled)
{
    if (enabled)
    {
        if (wifi_scan_task_handle != NULL || wifi_connect_task_handle != NULL)
        {
            ESP_LOGW(TAG, "Scan tasks are already running");
            return;
        }

        // 创建扫描任务
        xTaskCreate(wifi_scan_task, "wifi_scan_task", 1024 * 4, NULL, 1, &wifi_scan_task_handle);
        if (wifi_scan_task_handle == NULL)
        {
            ESP_LOGE(TAG, "Failed to create wifi_scan_task");
            return;
        }

        // 创建连接任务
        xTaskCreate(wifi_connect_task, "wifi_connect_task", 1024 * 4, NULL, 1, &wifi_connect_task_handle);
        if (wifi_connect_task_handle == NULL)
        {
            ESP_LOGE(TAG, "Failed to create wifi_connect_task");
            vTaskDelete(wifi_scan_task_handle); // 删除扫描任务
            wifi_scan_task_handle = NULL;
            return;
        }

        ESP_LOGI(TAG, "Wi-Fi scan and connect tasks started");
        is_scan_enabled = enabled;
    }
    else
    {
        if (wifi_scan_task_handle == NULL && wifi_connect_task_handle == NULL)
        {
            ESP_LOGW(TAG, "Scan tasks are not running");
            return;
        }

        // 删除扫描任务
        if (wifi_scan_task_handle != NULL)
        {
            vTaskDelete(wifi_scan_task_handle);
            wifi_scan_task_handle = NULL;
        }

        // 删除连接任务
        if (wifi_connect_task_handle != NULL)
        {
            vTaskDelete(wifi_connect_task_handle);
            wifi_connect_task_handle = NULL;
        }

        // 清除扫描结果
        scan_result_count = 0;
        memset(scan_results, 0, sizeof(scan_results));

        ESP_LOGI(TAG, "Wi-Fi scan and connect tasks stopped");
    }
}

// 设置电源节省模式
void wifi_service_set_power_save_mode(wifi_ps_type_t mode)
{
    ESP_ERROR_CHECK(esp_wifi_set_ps(mode));
}

// 获取 Wi-Fi 开启状态
bool wifi_service_get_wifi_status(void)
{
    return is_wifi_enabled;
}

// 获取 Wi-Fi 扫描状态
bool wifi_service_get_scan_status(void)
{
    return is_scan_enabled;
}

// 获取 NVS 中存储的 Wi-Fi 配置或当前连接的 Wi-Fi 配置
bool wifi_service_get_wifi_config(wifi_config_t *config)
{
    if (config == NULL)
    {
        ESP_LOGE(TAG, "Invalid config pointer");
        return false;
    }

    // 如果当前已连接 Wi-Fi，则获取当前连接的配置
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
    {
        return get_current_wifi_config(config);
    }

    // 如果未连接 Wi-Fi，则从 NVS 中加载配置
    return load_wifi_config_from_nvs(config->sta.ssid, config->sta.password);
}

// 生成当前已连接的或 NVS 配置中的 Wi-Fi URL
bool wifi_service_generate_wifi_url(char *url, size_t url_size)
{
    if (url == NULL || url_size < 128)
    {
        ESP_LOGE(TAG, "Invalid URL buffer or size too small");
        return false;
    }

    wifi_config_t config;
    if (!wifi_service_get_wifi_config(&config))
    {
        ESP_LOGE(TAG, "Failed to get Wi-Fi config");
        return false;
    }

    // 获取指定 SSID 的详细信息
    wifi_ap_record_t ap_info;
    if (!get_ap_details(config.sta.ssid, &ap_info))
    {
        ESP_LOGE(TAG, "Failed to get AP details for SSID: %s", config.sta.ssid);
        return false;
    }

    // 获取认证模式
    const char *auth_type;
    switch (ap_info.authmode)
    {
    case WIFI_AUTH_WEP:
        auth_type = "WEP";
        break;
    case WIFI_AUTH_WPA_PSK:
        auth_type = "WPA";
        break;
    case WIFI_AUTH_WPA2_PSK:
        auth_type = "WPA2";
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        auth_type = "WPA2";
        break;
    default:
        auth_type = "nopass";
        break;
    }

    // 生成 Wi-Fi URL
    snprintf(url, url_size, "WIFI:T:%s;P:%s;S:%s;H:false;", auth_type, config.sta.password, config.sta.ssid);
    return true;
}

// 获取当前已连接 Wi-Fi 的 SSID
bool wifi_service_get_connected_ssid(char *ssid, size_t ssid_size)
{
    if (ssid == NULL || ssid_size < 33)
    {
        ESP_LOGE(TAG, "Invalid SSID buffer or size too small");
        return false;
    }

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get current AP info: %s", esp_err_to_name(err));
        return false;
    }

    strncpy(ssid, (char *)ap_info.ssid, ssid_size - 1);
    ssid[ssid_size - 1] = '\0'; // 确保字符串以 null 结尾
    return true;
}

// 获取 Wi-Fi 连接状态
wifi_connection_status_t wifi_service_get_connection_status(void)
{
    return connection_status;
}

// 获取当前是否正在扫描
bool wifi_service_is_scanning(void)
{
    return is_scanning;
}