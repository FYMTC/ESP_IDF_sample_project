#include "UI.h"
// 存储亮度的 NVS 键
#define BRIGHTNESS_KEY "brightness"

static const char *TAG = "LVGL_MENU";
extern LGFX_tft tft;
static lv_obj_t *main_screen;
static lv_obj_t *dropdown_screen;
static lv_obj_t *WIFI_screen;
static lv_obj_t *meter;
static lv_meter_indicator_t *indic0, *indic1, *indic2, *indic3; // Declare indicators globall
static lv_obj_t *ram_status_label;
static lv_obj_t *wifi_status_label;
static lv_obj_t *bt_status_label;
static lv_obj_t *brightness_slider;

lv_obj_t *qr;
lv_obj_t *wifi_list;
lv_timer_t *draw_timer;
char url[128]="WIFI:T:WPA;P:45678912;S:DESKTOP-A8791RO 6307;H:false;";
void wifi_back_menu_cb(lv_event_t *event);
void timer_draw_list_cb(lv_timer_t *);
void draw_wifi_list(void);
static void qrcode_frash_task(void *pvParameters);
static void textarea_event_handler(lv_event_t *e);
void WIFI_screen_back_btn(lv_event_t *e);
static void get_pasward(const char *ta_ssid);
void wifi_event_handler(lv_event_t *event);
void draw_list_cb(lv_timer_t *t);
extern bool complete_wifi_scan_flag;
char passward_ssid[33];
static void get_passward();
uint8_t count;
void draw_wifi_list_cb(lv_event_t *event);
TaskHandle_t draw_qrcode_task_handle;

// 更新系统信息
void update_system_info(_lv_timer_t *timer);
lv_timer_t *timer_update_system_info;

// 回调函数声明
void menu_action_WIFI(lv_event_t *e);
void menu_action_game1(lv_event_t *e);
void menu_action_game2(lv_event_t *e);
void menu_action_game3(lv_event_t *e);
void menu_action_game4(lv_event_t *e);
void menu_action_restart(lv_event_t *e);
void menu_action_off(lv_event_t *e);
void menu_action_settings(lv_event_t *e);
void menu_action_sdcard(lv_event_t *e);

// 回调函数声明
void gesture_handler(lv_event_t *e);
void switch_to_main_screen(void);
void switch_to_dropdown_screen(void);
void switch_to_WIFI_screen(void);
// 事件回调声明
void brightness_slider_event_cb(lv_event_t *e);
void dropdown_screen_backbtn_cb(lv_event_t *e);
// 初始化 NVS
void init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS initialization failed");
        return;
    }
}

// 读取 NVS 中的亮度值
int get_brightness_from_nvs()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for reading!");
        return 50; // 默认亮度值 50
    }

    int32_t brightness = 50; // 默认亮度
    err = nvs_get_i32(my_handle, BRIGHTNESS_KEY, &brightness);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read brightness from NVS, using default.");
        brightness = 50; // 默认亮度
    }

    nvs_close(my_handle);
    // 调用硬件接口调整屏幕亮度
    tft.setBrightness(brightness);
    return brightness;
}

// 保存亮度值到 NVS
void save_brightness_to_nvs(int brightness)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for saving!");
        return;
    }

    err = nvs_set_i32(my_handle, BRIGHTNESS_KEY, brightness);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write brightness to NVS.");
    }
    else
    {
        ESP_LOGI(TAG, "Brightness saved to NVS: %d", brightness);
    }

    nvs_commit(my_handle);
    nvs_close(my_handle);
}

void create_menu()
{
    // 创建主界面
    main_screen = lv_obj_create(NULL);

    // 创建一个列表对象
    lv_obj_t *list = lv_list_create(main_screen);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));

    // 为主界面添加手势识别事件
    // lv_obj_add_event_cb(main_screen, gesture_handler, LV_EVENT_GESTURE, NULL);
    // lv_obj_add_flag(list, LV_OBJ_FLAG_EVENT_BUBBLE);//事件冒泡

    // 添加菜单项
    lv_obj_t *btn;

    static lv_style_t style;
    lv_style_init(&style);

    lv_list_add_text(list, LV_SYMBOL_HOME "Home");
    // lv_obj_add_event_cb(btn, menu_action_1, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Settings");
    lv_obj_add_event_cb(btn, menu_action_settings, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, "WIFI");
    lv_obj_add_event_cb(btn, menu_action_WIFI, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_FILE, "cube game");
    lv_obj_add_event_cb(btn, menu_action_game1, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_FILE, "ball game");
    lv_obj_add_event_cb(btn, menu_action_game2, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_FILE, "pvz");
    lv_obj_add_event_cb(btn, menu_action_game3, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_FILE, "fly game");
    lv_obj_add_event_cb(btn, menu_action_game4, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_SD_CARD, "SD files");
    lv_obj_add_event_cb(btn, menu_action_sdcard, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_REFRESH, "Restart");
    lv_style_set_text_color(&style, lv_palette_main(LV_PALETTE_ORANGE));
    lv_obj_add_style(btn, &style, 0);
    lv_obj_add_event_cb(btn, menu_action_restart, LV_EVENT_CLICKED, NULL);

    btn = lv_list_add_btn(list, LV_SYMBOL_POWER, "OFF");
    lv_style_set_text_color(&style, lv_palette_main(LV_PALETTE_RED));
    lv_obj_add_style(btn, &style, 0);
    lv_obj_add_event_cb(btn, menu_action_off, LV_EVENT_CLICKED, NULL);
    // 切换到主界面
    lv_scr_load(main_screen);
}

void menu_action_settings(lv_event_t *e)
{
    switch_to_dropdown_screen();
    ESP_LOGI(TAG, "Home button clicked!");
}
void menu_action_WIFI(lv_event_t *e)
{
    switch_to_WIFI_screen();
    ESP_LOGI(TAG, "Home button clicked!");
}
void menu_action_game1(lv_event_t *e)
{
    cube_game_start();
    ESP_LOGI(TAG, "Home button clicked!");
}

void menu_action_game2(lv_event_t *e)
{
    ballgame_start();
    ESP_LOGI(TAG, "Files button clicked!");
}

void menu_action_game3(lv_event_t *e)
{
    pvz_start();
    ESP_LOGI(TAG, "Settings button clicked!");
}

void menu_action_game4(lv_event_t *e)
{
    fly_game_start();
    ESP_LOGI(TAG, "Settings button clicked!");
}

void menu_action_sdcard(lv_event_t *e)
{
    create_file_browser_ui();
    ESP_LOGI(TAG, "Settings button clicked!");
}

void menu_action_restart(lv_event_t *e)
{
    ESP_LOGI(TAG, "Restart button clicked!");
    esp_restart();
}

void menu_action_off(lv_event_t *e)
{
    ESP_LOGI(TAG, "Restart button clicked!");
    // 进入深度睡眠
    esp_deep_sleep_start();
}

// 创建下拉界面
void create_dropdown_screen(void)
{
    dropdown_screen = lv_obj_create(NULL);
    lv_obj_t *status = lv_obj_create(dropdown_screen);
    lv_obj_set_size(status, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_ver(status, 0, 0);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_COLUMN); // 按钮内容弹性行增长
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_width(status, 0, 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);

    // 添加屏幕手势
    // lv_obj_add_event_cb(dropdown_screen, gesture_handler, LV_EVENT_GESTURE, NULL);
    // lv_obj_add_event_cb(lv_scr_sys(), gesture_handler, LV_EVENT_GESTURE, NULL);
    // lv_obj_add_flag(status, LV_OBJ_FLAG_EVENT_BUBBLE);//事件冒泡

    lv_obj_t *dropdown_screen_backbtn = lv_btn_create(status);
    lv_obj_set_size(dropdown_screen_backbtn, LV_PCT(100), 20);
    lv_obj_t *img = lv_img_create(dropdown_screen_backbtn);
    lv_img_set_src(img, LV_SYMBOL_NEW_LINE);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(dropdown_screen_backbtn);
    lv_label_set_text(label, "BACK");
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(dropdown_screen_backbtn, dropdown_screen_backbtn_cb, LV_EVENT_CLICKED, NULL);

    // 添加饼图显示内存占用情况
    meter = lv_meter_create(status);

    lv_obj_set_size(meter, LV_HOR_RES * 0.2, LV_HOR_RES * 0.2);
    lv_obj_align(meter, LV_ALIGN_TOP_MID, 0, 20);

    /* Remove the background and the middle circle */
    lv_obj_remove_style(meter, NULL, LV_PART_MAIN);
    lv_obj_remove_style(meter, NULL, LV_PART_INDICATOR);

    /* Create scale */
    lv_meter_scale_t *scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 0, 0, 0, lv_color_black());
    lv_meter_set_scale_range(meter, scale, 0, 100, 360, 0);

    /* Create the arc indicators for memory usage */
    lv_coord_t indic_w = 100;

    indic0 = lv_meter_add_arc(meter, scale, indic_w, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_meter_set_indicator_start_value(meter, indic0, 0);
    lv_meter_set_indicator_end_value(meter, indic0, 100);

    indic1 = lv_meter_add_arc(meter, scale, indic_w, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_meter_set_indicator_start_value(meter, indic1, 0);
    lv_meter_set_indicator_end_value(meter, indic1, 40); // 40% memory usage

    indic2 = lv_meter_add_arc(meter, scale, indic_w, lv_palette_main(LV_PALETTE_YELLOW), 0);
    lv_meter_set_indicator_start_value(meter, indic2, 40); // Start from the previous
    lv_meter_set_indicator_end_value(meter, indic2, 80);   // 80% memory usage

    indic3 = lv_meter_add_arc(meter, scale, indic_w, lv_palette_main(LV_PALETTE_DEEP_ORANGE), 0);
    lv_meter_set_indicator_start_value(meter, indic3, 80); // Start from the previous
    lv_meter_set_indicator_end_value(meter, indic3, 100);  // 100% memory usage

    // 添加 内存 状态标签
    ram_status_label = lv_label_create(status);
    lv_label_set_text(ram_status_label, "used : Unknown");

    // 添加滑块调节屏幕亮度
    lv_obj_t *slider_label = lv_label_create(status);
    lv_label_set_text(slider_label, "Brightness:");
    brightness_slider = lv_slider_create(status);
    lv_obj_set_width(brightness_slider, 150);
    lv_slider_set_range(brightness_slider, 10, 255);
    lv_slider_set_value(brightness_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // 读取保存的亮度值
    int saved_brightness = get_brightness_from_nvs();
    // 根据保存的亮度设置屏幕亮度
    lv_slider_set_value(brightness_slider, saved_brightness, LV_ANIM_OFF);

    // 添加 WiFi 状态标签
    wifi_status_label = lv_label_create(status);
    lv_label_set_text(wifi_status_label, "WiFi: Unknown");

    // 添加蓝牙状态标签
    bt_status_label = lv_label_create(status);
    lv_label_set_text(bt_status_label, "Bluetooth: Unknown");

    // 启动定时器周期性更新系统信息
    timer_update_system_info = lv_timer_create(update_system_info, 1000, NULL); // 每秒更新一次
    lv_timer_set_repeat_count(timer_update_system_info, -1);
}
// 创建WiFi列表界面
void create_WIFI_screen(void)
{
    WIFI_screen = lv_obj_create(NULL);
    lv_obj_t *obj = lv_obj_create(WIFI_screen); // 在屏幕上创建一个横向弹性增长盒子
    lv_obj_set_size(obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(obj, 0, 0);                           // 无边距
    lv_obj_set_style_border_width(obj, 0, 0);                      // 无边框
    lv_obj_set_scrollbar_mode(WIFI_screen, LV_SCROLLBAR_MODE_OFF); // 无滚动条
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);                   // 横向增长
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_snap_x(obj, LV_SCROLL_SNAP_CENTER); // x方向捕捉
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *qr_cont = lv_obj_create(obj);
    lv_obj_set_size(qr_cont, LV_HOR_RES, LV_VER_RES);
    qr = lv_qrcode_create(qr_cont, lv_disp_get_hor_res(NULL) - 60, lv_color_black(), lv_color_white());
    lv_obj_center(qr);
    lv_qrcode_update(qr, url, strlen(url));

    wifi_list = lv_list_create(obj);
    lv_obj_set_style_text_font(wifi_list, &NotoSansSC_Medium_3500, 0);
    lv_obj_set_size(wifi_list, LV_HOR_RES, LV_VER_RES);
    // numNetworks = 0;
    // wifi_scan_flag = true;

    // UI刷新定时器
    draw_timer = lv_timer_create(timer_draw_list_cb, 1000, NULL);
    lv_timer_set_repeat_count(draw_timer, -1);

    wifi_service_set_scan_enabled(true); // 打开扫描
    complete_wifi_scan_flag = true;
    // if (wifi_service_get_wifi_status() && wifi_service_generate_wifi_url(url, sizeof(url)))
    // {
    //     ESP_LOGI(TAG, "Wi-Fi URL: %s", url);
    // }
    // else
    // {
    //     ESP_LOGE(TAG, "Failed to generate Wi-Fi URL");
    // }
    
    xTaskCreate(qrcode_frash_task, "qrcode_frash_task", 1024*4, NULL, 1, &draw_qrcode_task_handle);

    lv_obj_t *label = lv_label_create(qr_cont);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 15);
    lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(label, LV_PCT(90));
    lv_label_set_text(label, url);
    char security[8]; // 存储安全类型
    char pd[20];      // 存储密码
    char ssid[40];    // 存储SSID
    char hidden[10];  // 存储隐藏状态
    // 使用sscanf从字符串中提取数据并存储到变量中
    sscanf(url, "WIFI:T:%[^;];P:%[^;];S:%[^;];H:%[^;];", security, pd, ssid, hidden);
    lv_obj_t *label2 = lv_label_create(qr_cont);
    lv_obj_align_to(label2, qr, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);
    lv_label_set_text_fmt(label2, "ssid:%s\npassward:%s", ssid, pd);
    lv_scr_load(WIFI_screen);
}
static void qrcode_frash_task(void *pvParameters){
    if (wifi_service_get_wifi_status() && wifi_service_generate_wifi_url(url, sizeof(url)))
    {
        ESP_LOGI(TAG, "Wi-Fi URL: %s", url);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to generate Wi-Fi URL");
    }
    lv_qrcode_update(qr, url, strlen(url));
    vTaskDelete(NULL);
}
// 定时器刷新WiFi列表
void timer_draw_list_cb(lv_timer_t *t)
{

    if (complete_wifi_scan_flag || (count == 0))
    {
        draw_wifi_list();
        printf("wifi wifi_list reflash\n");
        complete_wifi_scan_flag = false;
        printf("wifi complete_wifi_scan_flag false\n");
    }
}
// 切换到WIFI界面
void dropdown_screen_backbtn_cb(lv_event_t *e)
{
    switch_to_main_screen();
}
void draw_wifi_list_cb(lv_event_t *event)
{
    draw_wifi_list();
}
// 绘制WiFi列表，显示当前连接状态
void draw_wifi_list()
{

    lv_obj_clean(wifi_list);
    lv_obj_t *btn = lv_list_add_btn(wifi_list, LV_SYMBOL_NEW_LINE, "back");
    lv_obj_add_event_cb(btn, wifi_back_menu_cb, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_btn(wifi_list, LV_SYMBOL_REFRESH, "REFRESH");
    lv_obj_add_event_cb(btn, draw_wifi_list_cb, LV_EVENT_CLICKED, NULL);
    char *wifi_status;
    if (wifi_service_get_wifi_status()) // wifi打开状态
    {
        wifi_status = "wifi enabled";
        btn = lv_list_add_text(wifi_list, wifi_status);
    }
    else
    {
        wifi_status = "wifi disabled";
        btn = lv_list_add_text(wifi_list, wifi_status);
    }

    wifi_scan_result_t results[MAX_SCAN_RESULTS];
    count = wifi_service_get_scan_results(results, MAX_SCAN_RESULTS);
    if (count == 0)
    {
        lv_obj_t *spinner = lv_spinner_create(wifi_list, 1000, 60);
        lv_obj_set_size(spinner, LV_PCT(20), LV_PCT(20));
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            lv_obj_t *btn = lv_list_add_btn(wifi_list, LV_SYMBOL_WIFI, results[i].ssid);
            lv_obj_add_event_cb(btn, wifi_event_handler, LV_EVENT_CLICKED, NULL); // 点击wifi列表弹出键盘
            if (wifi_service_get_wifi_status())
            {
                char ssid[33];
                wifi_service_get_connected_ssid(ssid, sizeof(ssid));
                if (strcmp(ssid, results[i].ssid) == 0)
                {
                    lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_LIGHT_BLUE), 0);
                }
            }
        }
    }
}
// 点击WiFi列表
void wifi_event_handler(lv_event_t *event)
{ // 连接WiFi，生成WiFi连接URL
    lv_obj_t *obj = lv_event_get_target(event);

    snprintf(passward_ssid, sizeof(passward_ssid), lv_list_get_btn_text(wifi_list, obj));
    wifi_scan_result_t results[MAX_SCAN_RESULTS];
    uint8_t count = wifi_service_get_scan_results(results, MAX_SCAN_RESULTS);
    for (int i = 0; i < count; i++)
    {
        printf("%d %s \r\n", i, results[i].ssid);
        if (strcmp(passward_ssid, results[i].ssid) == 0)
        {
            printf("GET WiFi.SSID:%d %s\r\n", i, results[i].ssid);
            // 获取密码
            // if (results[i].auth_mode != WIFI_AUTH_OPEN)
            //{
            get_passward();
            /*Create a keyboard to use it with an of the text areas*/
            //}

            break;
        }
    }
}
// 密码输入页面
static void get_passward()
{
    lv_obj_t *getnum = lv_obj_create(NULL);

    lv_obj_t *btn = lv_btn_create(getnum);
    lv_obj_set_size(btn, LV_PCT(100), 20);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *img = lv_img_create(btn);
    lv_img_set_src(img, LV_SYMBOL_NEW_LINE);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "BACK");
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

    lv_obj_t *kb = lv_keyboard_create(getnum);

    /*Create a text area. The keyboard will write here*/
    lv_obj_t *ta = lv_textarea_create(getnum);
    lv_obj_set_size(ta, LV_PCT(90), LV_PCT(30));
    lv_obj_align_to(ta, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_textarea_set_placeholder_text(ta, "PASSWORD");
    lv_textarea_set_one_line(ta, true);
    lv_keyboard_set_textarea(kb, ta);
    // static uint8_t num0=num;
    lv_obj_add_event_cb(ta, textarea_event_handler, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(btn, WIFI_screen_back_btn, LV_EVENT_CLICKED, getnum);

    lv_disp_load_scr(getnum);
}
// 返回到wifi列表界面
void WIFI_screen_back_btn(lv_event_t *e)
{
    complete_wifi_scan_flag = true;
    lv_obj_t *obj = (lv_obj_t *)e->user_data;
    lv_disp_load_scr(WIFI_screen);
    lv_obj_del(obj);
}
static void textarea_event_handler(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *getnum = lv_obj_get_parent(ta);

    const char *password = lv_textarea_get_text(ta);
    wifi_service_connect(passward_ssid, password);

    printf("ssid: %s\r\n", passward_ssid);
    printf("password: %s\r\n", password);
    printf("Connecting to WiFi...");

    wifi_connection_status_t status = wifi_service_get_connection_status();
    int i = 0;
    while (status != WIFI_CONNECTION_STATUS_CONNECTED && i < 100)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf(".");
        status = wifi_service_get_connection_status();
        i++;
    }
    printf("\n");

    switch (status)
    {
    case WIFI_CONNECTION_STATUS_DISCONNECTED:
        ESP_LOGI(TAG, "Wi-Fi status: Disconnected");
        break;
    case WIFI_CONNECTION_STATUS_CONNECTING:
        ESP_LOGI(TAG, "Wi-Fi status: Connecting");
        break;
    case WIFI_CONNECTION_STATUS_CONNECTED:
        ESP_LOGI(TAG, "Wi-Fi status: Connected");
        break;
    case WIFI_CONNECTION_STATUS_FAILED:
        ESP_LOGI(TAG, "Wi-Fi status: Failed");
        break;
    }

    complete_wifi_scan_flag = true; // 更新list
    lv_disp_load_scr(WIFI_screen);
    lv_obj_del(getnum);
}
// 返回主菜单
void wifi_back_menu_cb(lv_event_t *event)
{
    printf("[page_wifi] page_event_cb wifi->manu\n");
    lv_timer_del(draw_timer);
    wifi_service_set_scan_enabled(false);
    lv_obj_clean(WIFI_screen);
    lv_disp_load_scr(main_screen);
    lv_obj_del(WIFI_screen);
}
// 手势事件处理
void gesture_handler(lv_event_t *e)
{
    lv_dir_t gesture = lv_indev_get_gesture_dir(lv_indev_get_act());

    if (gesture == LV_DIR_LEFT)
    {
        ESP_LOGI(TAG, "Gesture: Swipe Down");
        switch_to_dropdown_screen();
        lv_timer_resume(timer_update_system_info);
    }
    else if (gesture == LV_DIR_RIGHT)
    {
        ESP_LOGI(TAG, "Gesture: Swipe Up");
        switch_to_main_screen();
        lv_timer_pause(timer_update_system_info);
    }
}

// 滑块事件回调，调节屏幕亮度
void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t brightness = (uint8_t)lv_slider_get_value(slider);

    ESP_LOGI(TAG, "Adjusting brightness to %d%%", brightness * 100 / 255);
    // 调用硬件接口调整屏幕亮度
    tft.setBrightness(brightness);
}

// 更新系统信息回调函数
void update_system_info(_lv_timer_t *timer)
{
    // 获取内存信息
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);

    // size_t free_mem = heap_info.total_free_bytes;
    // size_t used_mem = heap_info.total_allocated_bytes;
    // size_t total_mem = free_mem + used_mem;
    size_t free_mem = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    size_t total_mem = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024;
    size_t used_mem = total_mem - free_mem;

    // 更新饼图数据
    lv_meter_set_indicator_end_value(meter, indic1, used_mem * 100 / total_mem);
    lv_meter_set_indicator_end_value(meter, indic2, (used_mem * 100 / total_mem) > 40 ? (used_mem * 100 / total_mem) : 40); // Ensure the range
    lv_meter_set_indicator_end_value(meter, indic3, (used_mem * 100 / total_mem) > 80 ? (used_mem * 100 / total_mem) : 80); // Ensure the range

    lv_label_set_text_fmt(ram_status_label, "ram used: %dkbit\ntotal free head: \n%dkbit", used_mem, heap_info.total_free_bytes / 1024);

    // 更新 WiFi 状态
    wifi_mode_t wifi_mode;
    esp_wifi_get_mode(&wifi_mode);
    const char *wifi_status = (wifi_mode != WIFI_MODE_NULL) ? "Connected" : "Disconnected";
    lv_label_set_text_fmt(wifi_status_label, "WiFi: %s", wifi_status);

    // 更新蓝牙状态
    bool bt_enabled = esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED;
    const char *bt_status = bt_enabled ? "Enabled" : "Disabled";
    lv_label_set_text_fmt(bt_status_label, "Bluetooth: %s", bt_status);
}
// 切换到WiFi列表界面
void switch_to_WIFI_screen(void)
{
    create_WIFI_screen();
}
// 切换到下拉界面
void switch_to_dropdown_screen(void)
{
    if (dropdown_screen == NULL)
    {
        create_dropdown_screen();
    }
    lv_scr_load_anim(dropdown_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 500, 0, false);
}

// 切换回主界面
void switch_to_main_screen(void)
{
    save_brightness_to_nvs(lv_slider_get_value(brightness_slider));
    lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 0, false);
}

extern TaskHandle_t GPIOtask_handle;
extern TaskHandle_t bt_hid_taskhandle;
extern TaskHandle_t wifi_scan_task_handle;
extern TaskHandle_t wifi_connect_task_handle;
void print_ram_info()
{
    // 获取内部 RAM 的内存信息
    size_t free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024;
    size_t used_size = total_size - free_size;

    ESP_LOGI(TAG, "Internal RAM:");
    ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
    ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
    ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);

    // 获取 PSRAM 的内存信息（如果可用）
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0)
    {
        free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
        total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024;
        used_size = total_size - free_size;

        ESP_LOGI(TAG, "PSRAM:");
        ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
        ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
        ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);
    }
    else
    {
        ESP_LOGI(TAG, "PSRAM not available");
    }

    UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG,"print_ram_info: %u", high_water_mark);
    if (GPIOtask_handle != NULL)
    {
        high_water_mark = uxTaskGetStackHighWaterMark(GPIOtask_handle);
        ESP_LOGI(TAG,"GPIOtask_handle: %u", high_water_mark);
    }
    if (bt_hid_taskhandle != NULL)
    {
        high_water_mark = uxTaskGetStackHighWaterMark(bt_hid_taskhandle);
        ESP_LOGI(TAG,"bt_hid_task: %u", high_water_mark);
    }
    if (wifi_scan_task_handle != NULL)
    {
        high_water_mark = uxTaskGetStackHighWaterMark(wifi_scan_task_handle);
        ESP_LOGI(TAG,"wifi_scan_task: %u", high_water_mark);
    }
    if (wifi_connect_task_handle != NULL)
    {
        high_water_mark = uxTaskGetStackHighWaterMark(wifi_connect_task_handle);
        ESP_LOGI(TAG,"wifi_connect_task: %u", high_water_mark);
    }

}