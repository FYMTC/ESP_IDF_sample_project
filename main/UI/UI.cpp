#include "UI.h"
// 存储亮度的 NVS 键
#define BRIGHTNESS_KEY "brightness"

static const char *TAG = "LVGL_MENU";
extern LGFX_tft tft;
static lv_obj_t *main_screen;
static lv_obj_t *dropdown_screen;
static lv_obj_t *meter;
static lv_meter_indicator_t *indic1, *indic2, *indic3; // Declare indicators globall
static lv_obj_t *ram_status_label;
static lv_obj_t *wifi_status_label;
static lv_obj_t *bt_status_label;
static lv_obj_t *brightness_slider;

// 更新系统信息
void update_system_info(_lv_timer_t *timer);
lv_timer_t *timer_update_system_info;

// 回调函数声明
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
// 事件回调声明
void brightness_slider_event_cb(lv_event_t *e);
void dropdown_screen_backbtn_cb(lv_event_t *e);
// 初始化 NVS
esp_err_t init_nvs() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

// 读取 NVS 中的亮度值
int get_brightness_from_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for reading!");
        return 50;  // 默认亮度值 50
    }

    int32_t brightness = 50; // 默认亮度
    err = nvs_get_i32(my_handle, BRIGHTNESS_KEY, &brightness);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read brightness from NVS, using default.");
        brightness = 50; // 默认亮度
    }

    nvs_close(my_handle);
    // 调用硬件接口调整屏幕亮度
    tft.setBrightness(brightness);
    return brightness;
}

// 保存亮度值到 NVS
void save_brightness_to_nvs(int brightness) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle for saving!");
        return;
    }

    err = nvs_set_i32(my_handle, BRIGHTNESS_KEY, brightness);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write brightness to NVS.");
    } else {
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

    lv_list_add_text(list, LV_SYMBOL_HOME"Home");
    //lv_obj_add_event_cb(btn, menu_action_1, LV_EVENT_CLICKED, NULL);
    btn = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Settings");
    lv_obj_add_event_cb(btn, menu_action_settings, LV_EVENT_CLICKED, NULL);

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

    //添加屏幕手势
    //lv_obj_add_event_cb(dropdown_screen, gesture_handler, LV_EVENT_GESTURE, NULL);
    //lv_obj_add_event_cb(lv_scr_sys(), gesture_handler, LV_EVENT_GESTURE, NULL);
    //lv_obj_add_flag(status, LV_OBJ_FLAG_EVENT_BUBBLE);//事件冒泡

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

    lv_obj_set_size(meter, LV_HOR_RES*0.2, LV_HOR_RES*0.2);
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
    timer_update_system_info=lv_timer_create(update_system_info, 1000, NULL); // 每秒更新一次
    lv_timer_set_repeat_count(timer_update_system_info, -1);

    

}
void dropdown_screen_backbtn_cb(lv_event_t *e){
    switch_to_main_screen();
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

    size_t free_mem = heap_info.total_free_bytes;
    size_t used_mem = heap_info.total_allocated_bytes;
    size_t total_mem = free_mem + used_mem;

    // 更新饼图数据
    lv_meter_set_indicator_end_value(meter, indic1, used_mem * 100 / total_mem);
    lv_meter_set_indicator_end_value(meter, indic2, (used_mem * 100 / total_mem) > 40 ? (used_mem * 100 / total_mem) : 40); // Ensure the range
    lv_meter_set_indicator_end_value(meter, indic3, (used_mem * 100 / total_mem) > 80 ? (used_mem * 100 / total_mem) : 80); // Ensure the range

    lv_label_set_text_fmt(ram_status_label, "used: %dkbit", used_mem/1024);

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


void print_ram_info(){
    // 获取内部 RAM 的内存信息
    size_t free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL)/1024;
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL)/1024;
    size_t used_size = total_size - free_size;

    ESP_LOGI(TAG, "Internal RAM:");
    ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
    ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
    ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);

    // 获取 PSRAM 的内存信息（如果可用）
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0) {
        free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024;
        total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM)/1024;
        used_size = total_size - free_size;

        ESP_LOGI(TAG, "PSRAM:");
        ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
        ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
        ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);
    } else {
        ESP_LOGI(TAG, "PSRAM not available");
    }
}