#if 1
#include "bt.hpp"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "blecent.h"
#include "host/ble_store.h"

static const char *tag = "NimBLE_BLE_CENT";
static int blecent_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t peer_addr[6];
static uint16_t mouse_input_report_handle = 0; // 初始化为 0

//void ble_store_config_init(void);

static int
blecent_on_read(uint16_t conn_handle,
                const struct ble_gatt_error *error,
                struct ble_gatt_attr *attr,
                void *arg)
{
    if (error->status == 0) {
        MODLOG_DFLT(INFO, "Read complete; attr_handle=%d value=", attr->handle);
        print_mbuf(attr->om); // 打印接收到的数据
    }
    return 0;
}

static void
blecent_on_disc_complete(const struct peer *peer, int status, void *arg)
{
    if (status != 0) {
        MODLOG_DFLT(ERROR, "Error: Service discovery failed; status=%d "
                    "conn_handle=%d\n", status, peer->conn_handle);
        ble_gap_terminate(peer->conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return;
    }

    MODLOG_DFLT(INFO, "Service discovery complete; status=%d "
                "conn_handle=%d\n", status, peer->conn_handle);

    // 定义 UUID 变量
    ble_uuid16_t hid_service_uuid = BLE_UUID16_INIT(0x1812); // HID 服务 UUID
    ble_uuid16_t report_map_uuid = BLE_UUID16_INIT(0x2A4D);  // Report Map UUID
    ble_uuid16_t cccd_uuid = BLE_UUID16_INIT(0x2902);        // CCCD UUID

    // 查找特征
    const struct peer_chr *chr = peer_chr_find_uuid(peer,
                                                    &hid_service_uuid.u, // 传递 ble_uuid_t* 类型
                                                    &report_map_uuid.u); // 传递 ble_uuid_t* 类型
    if (chr != NULL) {
        MODLOG_DFLT(INFO, "Found HID service and Report characteristic\n");
        // 保存输入报告句柄
        mouse_input_report_handle = chr->chr.val_handle;
        MODLOG_DFLT(INFO, "Input report handle: %d\n", mouse_input_report_handle);

        // 查找 CCCD 描述符
        const struct peer_dsc *dsc = peer_dsc_find_uuid(peer,
                                                        &hid_service_uuid.u, // HID 服务 UUID
                                                        &report_map_uuid.u,  // Report Map UUID
                                                        &cccd_uuid.u);       // CCCD UUID
        if (dsc != NULL) {
            MODLOG_DFLT(INFO, "Found CCCD descriptor\n");
            // 启用通知
            uint8_t value[2] = {0x01, 0x00}; // 0x0001 表示启用通知
            int rc = ble_gattc_write_flat(peer->conn_handle, dsc->dsc.handle,
                                          value, sizeof(value), NULL, NULL);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to enable notification; rc=%d\n", rc);
            } else {
                MODLOG_DFLT(INFO, "Notification enabled\n");
            }
        } else {
            MODLOG_DFLT(ERROR, "CCCD not found\n");
        }
    } else {
        MODLOG_DFLT(ERROR, "Failed to find HID service or Report characteristic\n");
    }
}

static void
blecent_scan(void)
{
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    disc_params.filter_duplicates = 1;
    disc_params.passive = 1;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                      blecent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error initiating GAP discovery procedure; rc=%d\n",
                    rc);
    }
}

static int
blecent_should_connect(const struct ble_gap_disc_desc *disc)
{
    struct ble_hs_adv_fields fields;
    int rc;

    rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        return 0;
    }

    for (int i = 0; i < fields.num_uuids16; i++) {
        if (ble_uuid_u16(&fields.uuids16[i].u) == 0x1812) { // HID Service UUID
            return 1;
        }
    }

    return 0;
}

static void
blecent_connect_if_interesting(void *disc)
{
    uint8_t own_addr_type;
    int rc;
    ble_addr_t *addr = &((struct ble_gap_disc_desc *)disc)->addr;

    if (!blecent_should_connect((struct ble_gap_disc_desc *)disc)) {
        return;
    }

    rc = ble_gap_disc_cancel();
    if (rc != 0) {
        MODLOG_DFLT(DEBUG, "Failed to cancel scan; rc=%d\n", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    rc = ble_gap_connect(own_addr_type, addr, 30000, NULL,
                         blecent_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error: Failed to connect to device; addr_type=%d "
                    "addr=%s; rc=%d\n",
                    addr->type, addr_str(addr->val), rc);
        return;
    }
}

static int
blecent_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        blecent_connect_if_interesting(&event->disc);
        return 0;

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            MODLOG_DFLT(INFO, "Connection established ");

            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");

            rc = peer_add(event->connect.conn_handle);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to add peer; rc=%d\n", rc);
                return 0;
            }

            rc = peer_disc_all(event->connect.conn_handle,
                               blecent_on_disc_complete, NULL);
            if (rc != 0) {
                MODLOG_DFLT(ERROR, "Failed to discover services; rc=%d\n", rc);
                return 0;
            }
        } else {
            MODLOG_DFLT(ERROR, "Error: Connection failed; status=%d\n",
                        event->connect.status);
            blecent_scan();
        }

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
        print_conn_desc(&event->disconnect.conn);
        MODLOG_DFLT(INFO, "\n");

        peer_delete(event->disconnect.conn.conn_handle);
        blecent_scan();
        return 0;

    case BLE_GAP_EVENT_NOTIFY_RX:
         // 处理鼠标的输入报告
        if (event->notify_rx.attr_handle == mouse_input_report_handle) {
            uint8_t data[128];
            int len = os_mbuf_copydata(event->notify_rx.om, 0, sizeof(data), data);
            if (len > 0) {
                // 解析鼠标位移数据
                int dx = data[1]; // X 轴位移
                int dy = data[3]; // Y 轴位移
                bool leftButton = data[0] & 0x01; // 左键状态
                bool rightButton = data[0] & 0x02; // 右键状态
                MODLOG_DFLT(INFO, "Mouse movement: dx=%d, dy=%d, leftButton=%d, rightButton=%d\n", dx, dy, leftButton, rightButton);
            }
        }
        return 0;

    default:
        return 0;
    }
}

static void
blecent_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void
blecent_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    blecent_scan();
}

void blecent_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void bt_start(void)
{


    // 配置 NimBLE 主机
    ble_hs_cfg.reset_cb = blecent_on_reset;
    ble_hs_cfg.sync_cb = blecent_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    

    // 初始化 NimBLE 协议栈
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(tag, "Failed to init NimBLE: %d", ret);
        return;
    }
    
    int rc = peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
    assert(rc == 0);

    rc = ble_svc_gap_device_name_set("nimble-blecent");
    assert(rc == 0);

// 启动 NimBLE 主机任务
    nimble_port_freertos_init(blecent_host_task);
}


#endif