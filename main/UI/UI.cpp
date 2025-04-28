#include "UI.h"
#include "conf.h"
static const char *TAG = "LVGL_MENU";
lv_obj_t *main_screen;

extern lv_timer_t *timer_update_system_info;
extern lv_obj_t *brightness_slider;
extern lv_obj_t *dropdown_screen;


#if USE_ENCODER
extern lv_indev_t *encoder_indev;
#endif

// 回调函数声明
void switch_to_dropdown_screen(void);
// 定义菜单项结构体
typedef struct {
    const char *icon;
    const char *title;
    void (*action)(void); // 使用函数指针实现多态
} MenuItem;

// 菜单项数组
static const MenuItem menu_items[] = {
    {LV_SYMBOL_SETTINGS, "Settings", []() { switch_to_dropdown_screen(); ESP_LOGI(TAG, "Settings clicked!"); }},
    {LV_SYMBOL_WIFI, "WIFI", []() { create_WIFI_screen(); ESP_LOGI(TAG, "WIFI clicked!"); }},
    {"\xEF\x80\x97", "TIME", []() { create_time_page(); ESP_LOGI(TAG, "TIME clicked!"); }},
    {"\xEF\x84\xA4", "MPU6050", []() { create_mpu_page(); ESP_LOGI(TAG, "MPU6050 clicked!"); }},
    {"\xEF\x80\x84", "MAX30105", []() { create_heart_page(); ESP_LOGI(TAG, "MAX30105 clicked!"); }},
    {LV_SYMBOL_FILE, "cube game", []() { cube_game_start(); ESP_LOGI(TAG, "Cube Game clicked!"); }},
    {LV_SYMBOL_FILE, "ball game", []() { ballgame_start(); ESP_LOGI(TAG, "Ball Game clicked!"); }},
    {LV_SYMBOL_FILE, "pvz", []() { pvz_start(); ESP_LOGI(TAG, "PVZ clicked!"); }},
    {LV_SYMBOL_FILE, "fly game", []() { fly_game_start(); ESP_LOGI(TAG, "Fly Game clicked!"); }},
    {LV_SYMBOL_SD_CARD, "SD files", []() { create_file_browser_ui(); ESP_LOGI(TAG, "SD Files clicked!"); }},
    {LV_SYMBOL_REFRESH, "Restart", []() { ESP_LOGI(TAG, "Restart clicked!"); esp_restart(); }},
    {LV_SYMBOL_POWER, "OFF", []() { ESP_LOGI(TAG, "OFF clicked!"); esp_deep_sleep_start(); }},
};

// 初始化样式
static void init_styles(lv_style_t *style) {
    lv_style_init(style);
    lv_style_set_text_font(style, &NotoSansSC_Medium_3500);
}

// 通用事件回调函数
static void menu_event_handler(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    const MenuItem *item = (const MenuItem *)lv_event_get_user_data(e);
    if (item && item->action) {
        item->action(); // 调用对应的函数指针
    }
}

// 创建菜单
void create_menu() {
    main_screen = lv_obj_create(NULL);
    lv_obj_t *list = lv_list_create(main_screen);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));

    #if USE_ENCODER
    lv_group_t *group = lv_group_create();
    lv_indev_set_group(encoder_indev, group);
    #endif

    static lv_style_t style;
    init_styles(&style);

    for (size_t i = 0; i < sizeof(menu_items) / sizeof(menu_items[0]); i++) {
        lv_obj_t *btn = lv_list_add_btn(list, menu_items[i].icon, menu_items[i].title);
        lv_obj_add_event_cb(btn, menu_event_handler, LV_EVENT_CLICKED, (void *)&menu_items[i]);
        lv_obj_add_style(btn, &style, 0);
        #if USE_ENCODER
        lv_group_add_obj(group, btn);
        #endif

        // 特殊样式处理
        if (strcmp(menu_items[i].title, "Restart") == 0) {
            lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_GREEN), 0);
        } else if (strcmp(menu_items[i].title, "OFF") == 0) {
            lv_obj_set_style_text_color(btn, lv_palette_main(LV_PALETTE_RED), 0);
        }
    }

    lv_scr_load(main_screen);
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