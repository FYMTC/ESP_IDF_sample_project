#include "UI.h"

static const char *TAG = "LVGL_MENU";
lv_obj_t *main_screen;
// WiFi页面
lv_obj_t *qr;
lv_obj_t *wifi_list;
lv_timer_t *draw_timer;
char url[128] = "WIFI:T:WPA;P:45678912;S:DESKTOP-A8791RO 6307;H:false;";
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

extern lv_timer_t *timer_update_system_info;
extern lv_obj_t *brightness_slider;
extern lv_obj_t *dropdown_screen;

lv_obj_t *WIFI_screen;

// 回调函数声明
void menu_action_WIFI(lv_event_t *e);
void menu_action_TIME(lv_event_t *e);
void menu_action_HTTP(lv_event_t *e);
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
void switch_to_TIME_screen(void);
// 事件回调声明

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

    btn = lv_list_add_btn(list, MY_FONT_TIME, "TIME");
    lv_obj_add_event_cb(btn, menu_action_TIME, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_text_font(btn, &NotoSansSC_Medium_3500, 0);

    btn = lv_list_add_btn(list, MY_FONT_TIME, "HTTP");
    lv_obj_add_event_cb(btn, menu_action_HTTP, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_text_font(btn, &NotoSansSC_Medium_3500, 0);

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
void menu_action_TIME(lv_event_t *e)
{
    switch_to_TIME_screen();
    ESP_LOGI(TAG, "Home button clicked!");
}
void menu_action_HTTP(lv_event_t *e)
{
    //htpp_weather();
    ESP_LOGI(TAG, "HTTP button clicked!");
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

    xTaskCreate(qrcode_frash_task, "qrcode_frash_task", 1024 * 4, NULL, 1, &draw_qrcode_task_handle);

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
static void qrcode_frash_task(void *pvParameters)
{
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
    char wifi_status[20];
    if (wifi_service_get_wifi_status()) // wifi打开状态
    {
        strcpy(wifi_status, "wifi enabled");
        btn = lv_list_add_text(wifi_list, wifi_status);

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
    else
    {
        strcpy(wifi_status, "wifi disabled");
        btn = lv_list_add_text(wifi_list, wifi_status);
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

// 切换到WiFi列表界面
void switch_to_WIFI_screen(void)
{
    create_WIFI_screen();
}
// 切换到TIME列表界面
void switch_to_TIME_screen(void)
{
    create_time_page();
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

// // 定义运行时统计时钟
// extern uint32_t esp_get_ccount(void);
// #define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() (esp_get_ccount())
// #define portGET_RUN_TIME_COUNTER_VALUE() (esp_get_ccount())

// 输出内存状态信息

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

    TaskStatus_t *taskStatusArray = NULL;
    UBaseType_t taskCount, index;
    uint32_t totalRunTime;

    // 获取当前任务数量
    taskCount = uxTaskGetNumberOfTasks();

    // 分配内存来存储任务状态
    taskStatusArray = (TaskStatus_t *)pvPortMalloc(taskCount * sizeof(TaskStatus_t));

    if (taskStatusArray != NULL)
    {
        // 获取任务状态信息
        taskCount = uxTaskGetSystemState(taskStatusArray, taskCount, &totalRunTime);

        if (taskCount > 0)
        {
            // 打印表头
            printf("Task Name\t\tHigh Water Mark\tCPU Usage\n");
            printf("--------------------------------------------\n");

            // 打印每个任务的高水位线和 CPU 占用率
            for (index = 0; index < taskCount; index++)
            {
                // 获取任务的栈高水位线
                UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(taskStatusArray[index].xHandle);

                // 计算 CPU 占用率
                uint32_t cpuUsage = 0;
                if (totalRunTime > 0)
                {
                    cpuUsage = (taskStatusArray[index].ulRunTimeCounter * 100) / totalRunTime;
                }

                // 打印任务信息
                printf("%-16s\t%u\t\t%lu%%\n",
                       taskStatusArray[index].pcTaskName,
                       highWaterMark,
                       cpuUsage);
            }
        }
        else
        {
            printf("Failed to get task state information\n");
        }

        // 释放内存
        vPortFree(taskStatusArray);
    }
    else
    {
        printf("Failed to allocate memory for task status array\n");
    }
}