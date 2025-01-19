#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SCAN_RESULTS 20  // 最大扫描结果数量
#define SCAN_INTERVAL_MS 60000  // 扫描间隔时间（60秒）
#define NVS_WIFI_NAMESPACE "wifi_config"  // NVS 命名空间
#define NVS_KEY_WIFI_ENABLED "wifi_enabled"  // Wi-Fi 开关状态的键

// WiFi 扫描结果结构体
typedef struct {
    char ssid[33];  // SSID 最大长度为 32
    //int8_t rssi;    // 信号强度
    //wifi_auth_mode_t auth_mode;  // 认证模式
} wifi_scan_result_t;

// WiFi 配置结构体（重命名为 wifi_service_config_t）
// typedef struct {
//     char ssid[33];  // SSID 最大长度为 32
//     char password[65];  // 密码最大长度为 64
// } wifi_service_config_t;



// WiFi 连接状态枚举
typedef enum {
    WIFI_CONNECTION_STATUS_DISCONNECTED,  // 未连接
    WIFI_CONNECTION_STATUS_CONNECTING,    // 正在连接
    WIFI_CONNECTION_STATUS_CONNECTED,     // 已连接
    WIFI_CONNECTION_STATUS_FAILED         // 连接失败
} wifi_connection_status_t;

// WiFi 服务初始化
void wifi_service_init(void);

// 获取最近的扫描结果
uint8_t wifi_service_get_scan_results(wifi_scan_result_t *results, uint8_t max_results);

// 请求连接到指定 SSID
bool wifi_service_connect(const char *ssid, const char *password);

// 打开或关闭 Wi-Fi
void wifi_service_set_wifi_enabled(bool enabled);

// 打开或关闭 Wi-Fi 扫描
void wifi_service_set_scan_enabled(bool enabled);

// 设置电源节省模式
void wifi_service_set_power_save_mode(wifi_ps_type_t mode);

// 获取 Wi-Fi 开启状态
bool wifi_service_get_wifi_status(void);

// 获取 Wi-Fi 扫描状态
bool wifi_service_get_scan_status(void);

// 获取 NVS 中存储的 Wi-Fi 配置或当前连接的 Wi-Fi 配置
bool wifi_service_get_wifi_config(wifi_config_t *config);

// 生成当前已连接的或 NVS 配置中的 Wi-Fi URL
bool wifi_service_generate_wifi_url(char *url, size_t url_size);

// 获取当前已连接 Wi-Fi 的 SSID
bool wifi_service_get_connected_ssid(char *ssid, size_t ssid_size);

// 获取 Wi-Fi 连接状态
wifi_connection_status_t wifi_service_get_connection_status(void);

// 获取当前是否正在扫描
bool wifi_service_is_scanning(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_SERVICE_H