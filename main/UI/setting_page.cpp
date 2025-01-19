#include "UI.h"

// 更新系统信息
void update_system_info(_lv_timer_t *timer);
extern LGFX_tft tft;
extern lv_obj_t *main_screen;
lv_obj_t *dropdown_screen;

static const char *TAG = "SETTING_PAGE";

static lv_obj_t *meter;
static lv_meter_indicator_t *indic0, *indic1, *indic2, *indic3; // Declare indicators globall
static lv_obj_t *ram_status_label;
static lv_obj_t *wifi_status_label;
static lv_obj_t *bt_status_label;

lv_timer_t *timer_update_system_info;
lv_obj_t *tasks_label;
lv_obj_t *table;
lv_obj_t *wifi_sw;
lv_obj_t *ble_sw;
// 定义图表数据
lv_obj_t *chart;
static lv_chart_series_t *high_water_mark_series = NULL;
static lv_chart_series_t *cpu_usage_series = NULL;
void brightness_slider_event_cb(lv_event_t *e);
void dropdown_screen_backbtn_cb(lv_event_t *e);
lv_obj_t *brightness_slider;
static void draw_event_cb(lv_event_t *e);
static void draw_part_event_cb(lv_event_t *e);
#define configMAX_TASK_NAME_LEN 32
// 任务信息结构体，用于排序
typedef struct
{
    char taskName[configMAX_TASK_NAME_LEN];
    UBaseType_t highWaterMark;
    uint32_t cpuUsage;
} TaskInfo;

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

void save_switch_state(const char *key, bool state)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return;
    }

    err = nvs_set_i8(nvs_handle, key, state ? 1 : 0);
    if (err != ESP_OK)
    {
        printf("Error (%s) saving state to NVS!\n", esp_err_to_name(err));
    }

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

// 从nvs中读取开关状态
bool load_switch_state(const char *key)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return true; // 默认返回开状态
    }

    int8_t state = 0;
    err = nvs_get_i8(nvs_handle, key, &state);
    if (err != ESP_OK)
    {
        printf("Error (%s) reading state from NVS! Setting default state to ON.\n", esp_err_to_name(err));
        state = 1;                          // 默认设置为开状态
        nvs_set_i8(nvs_handle, key, state); // 将默认状态保存到 NVS
        nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return state == 1;
}
void set_switches()
{
    bool wifi_state = wifi_service_get_wifi_status();
    bool ble_state = load_switch_state("ble_sw");

    if (wifi_state)
    {
        lv_obj_add_state(wifi_sw, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_clear_state(wifi_sw, LV_STATE_CHECKED);
    }

    if (ble_state)
    {
        lv_obj_add_state(ble_sw, LV_STATE_CHECKED);
    }
    else
    {
        lv_obj_clear_state(ble_sw, LV_STATE_CHECKED);
    }
}
void wifi_sw_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
    save_switch_state("wifi_sw", state);
    wifi_service_set_wifi_enabled(state);
}
void ble_sw_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
    save_switch_state("ble_sw", state);
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

    ram_status_label = lv_label_create(status);

    // 添加 WiFi 状态标签
    wifi_status_label = lv_label_create(status);
    lv_label_set_text(wifi_status_label, "WiFi: Unknown");
    wifi_sw = lv_switch_create(status);
    lv_obj_add_event_cb(wifi_sw, wifi_sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 添加蓝牙状态标签
    bt_status_label = lv_label_create(status);
    lv_label_set_text(bt_status_label, "Bluetooth: Unknown");
    ble_sw = lv_switch_create(status);
    lv_obj_add_event_cb(ble_sw, ble_sw_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    set_switches();

    // 添加滑块调节屏幕亮度
    lv_obj_t *slider_label = lv_label_create(status);
    lv_label_set_text(slider_label, "\nBrightness:");
    brightness_slider = lv_slider_create(status);
    lv_obj_set_width(brightness_slider, 150);
    lv_slider_set_range(brightness_slider, 10, 255);
    lv_slider_set_value(brightness_slider, 50, LV_ANIM_OFF);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // 读取保存的亮度值
    int saved_brightness = get_brightness_from_nvs();
    // 根据保存的亮度设置屏幕亮度
    lv_slider_set_value(brightness_slider, saved_brightness, LV_ANIM_OFF);

    // 创建任务信息的图表对象
    chart = lv_chart_create(status);
    lv_obj_set_size(chart, LV_PCT(80), LV_PCT(60));

    // 设置图表类型为柱状图
    lv_chart_set_type(chart, LV_CHART_TYPE_BAR);

    // 设置范围
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 4096);
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);
    //  添加两个数据系列
    high_water_mark_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    cpu_usage_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_SECONDARY_Y);

    // // 设置 X 轴刻度线和标签
    // lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 4, 2, 5, 1, true, 40);
    // /*Zoom in a little in X*/
    // lv_chart_set_zoom_x(chart, uxTaskGetNumberOfTasks()*20);

    // 设置 Y 轴刻度线和标签
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 5, 2, 6, 2, true, 50);
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 5, 2, 6, 2, true, 50);

    // 添加自定义绘制事件回调
    // lv_obj_add_event_cb(chart, draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    // 添加 任务内存 状态标签
    tasks_label = lv_label_create(status);
    lv_label_set_text(tasks_label, "");

    // 添加 任务内存 列表
    table = lv_table_create(status);
    lv_table_set_cell_value(table, 0, 0, "TaskName");
    lv_table_set_cell_value(table, 0, 1, "HWM");
    lv_table_set_cell_value(table, 0, 2, "CPU%");
    lv_obj_set_width(table, DISP_HOR_RES - 20);
    lv_table_set_col_width(table, 0, (DISP_HOR_RES - 20) / 3);
    lv_table_set_col_width(table, 1, (DISP_HOR_RES - 20) / 3);
    lv_table_set_col_width(table, 2, (DISP_HOR_RES - 20) / 3);
    lv_obj_add_event_cb(table, draw_part_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);

    // 启动定时器周期性更新系统信息
    timer_update_system_info = lv_timer_create(update_system_info, 1000, NULL); // 每秒更新一次
    lv_timer_set_repeat_count(timer_update_system_info, -1);
}
// 自定义绘制事件回调函数
// 定义一个静态函数，用于处理绘图部分的事件回调
static void draw_part_event_cb(lv_event_t *e)
{
    // 获取事件目标对象，即触发该事件的LVGL对象
    lv_obj_t *obj = lv_event_get_target(e);
    // 获取绘图部分描述符，包含绘图相关的信息
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    /*If the cells are drawn...*/
    if (dsc->part == LV_PART_ITEMS)
    {
        uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
        uint32_t col = dsc->id - row * lv_table_get_col_cnt(obj);

        /*Make the texts in the first cell center aligned*/
        if (row == 0)
        {
            dsc->label_dsc->align = LV_TEXT_ALIGN_CENTER;
            dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_BLUE), dsc->rect_dsc->bg_color, LV_OPA_20);
            dsc->rect_dsc->bg_opa = LV_OPA_COVER;
        }
        /*In the first column align the texts to the right*/
        else if (col == 0)
        {
            dsc->label_dsc->align = LV_TEXT_ALIGN_RIGHT;
        }

        /*MAke every 2nd row grayish*/
        if ((row != 0 && row % 2) == 0)
        {
            dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_GREY), dsc->rect_dsc->bg_color, LV_OPA_10);
            dsc->rect_dsc->bg_opa = LV_OPA_COVER;
        }
    }
}
// 自定义绘制事件回调函数
static void draw_event_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (!lv_obj_draw_part_check_type(dsc, &lv_chart_class, LV_CHART_DRAW_PART_TICK_LABEL))
        return;

    // 自定义 X 轴标签
    if (dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text)
    {
        UBaseType_t taskCount, index;
        uint32_t totalRunTime;
        TaskStatus_t *taskStatusArray = NULL;
        // 获取当前任务数量
        taskCount = uxTaskGetNumberOfTasks();
        taskStatusArray = (TaskStatus_t *)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
        taskCount = uxTaskGetSystemState(taskStatusArray, taskCount, &totalRunTime);
        lv_snprintf(dsc->text, dsc->text_length, "%s", taskStatusArray[dsc->value].pcTaskName);
        vPortFree(taskStatusArray);
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
// 比较函数，用于按 highWaterMark 从大到小排序
int compare_task_info(const void *a, const void *b)
{
    TaskInfo *taskA = (TaskInfo *)a;
    TaskInfo *taskB = (TaskInfo *)b;
    return taskB->highWaterMark - taskA->highWaterMark;
}
// 更新系统信息回调函数
void update_system_info(_lv_timer_t *timer)
{
    // 获取内存信息
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_8BIT);

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

    // 获取任务状态信息
    uint32_t totalRunTime;
    TaskStatus_t *taskStatusArray = NULL;
    UBaseType_t taskCount, index;
    taskCount = uxTaskGetNumberOfTasks();
    taskStatusArray = (TaskStatus_t *)pvPortMalloc(taskCount * sizeof(TaskStatus_t));
    lv_chart_set_point_count(chart, taskCount);
    // 设置 X 轴刻度线和标签
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_X, 4, 2, taskCount, 1, true, 40);
    /*Zoom in a little in X*/
    lv_chart_set_zoom_x(chart, taskCount * 50);
    char temp[64]; // 临时存储每行信息
    if (taskStatusArray != NULL)
    {
        // 获取任务状态信息
        taskCount = uxTaskGetSystemState(taskStatusArray, taskCount, &totalRunTime);
        // 清空图表数据
        lv_chart_set_all_value(chart, high_water_mark_series, LV_CHART_POINT_NONE);
        lv_chart_set_all_value(chart, cpu_usage_series, LV_CHART_POINT_NONE);
        lv_obj_clean(table);
        if (taskCount > 0)
        {
            // 分配内存来存储排序后的任务信息
            TaskInfo *taskInfoArray = (TaskInfo *)pvPortMalloc(taskCount * sizeof(TaskInfo));
            if (taskInfoArray != NULL)
            {
                // 提取任务信息并存储到 taskInfoArray
                for (index = 0; index < taskCount; index++)
                {
                    strncpy(taskInfoArray[index].taskName, taskStatusArray[index].pcTaskName, configMAX_TASK_NAME_LEN);
                    taskInfoArray[index].highWaterMark = uxTaskGetStackHighWaterMark(taskStatusArray[index].xHandle);
                    // 计算 CPU 占用率
                    taskInfoArray[index].cpuUsage = 0;
                    if (totalRunTime > 0)
                    {
                        taskInfoArray[index].cpuUsage = (taskStatusArray[index].ulRunTimeCounter * 100) / totalRunTime;
                    }
                }
                // 按 highWaterMark 从大到小排序
                qsort(taskInfoArray, taskCount, sizeof(TaskInfo), compare_task_info);

                // 更新图表数据
                for (index = 0; index < taskCount; index++)
                {
                    // 获取任务的栈高水位线
                    UBaseType_t highWaterMark = taskInfoArray[index].highWaterMark;

                    lv_coord_t *ser1_array = lv_chart_get_y_array(chart, high_water_mark_series);
                    lv_coord_t *ser2_array = lv_chart_get_y_array(chart, cpu_usage_series);
                    ser1_array[index] = taskInfoArray[index].highWaterMark;
                    ser2_array[index] = taskInfoArray[index].cpuUsage;
                    lv_table_set_cell_value(table, index + 1, 0, taskInfoArray[index].taskName);
                    snprintf(temp, sizeof(temp), "%u", taskInfoArray[index].highWaterMark);
                    lv_table_set_cell_value(table, index + 1, 1, temp);
                    snprintf(temp, sizeof(temp), "%lu%%", taskInfoArray[index].cpuUsage);
                    lv_table_set_cell_value(table, index + 1, 2, temp);

                    // lv_chart_set_value_by_id(chart,high_water_mark_series,);
                }
                // 释放排序后的任务信息内存
                vPortFree(taskInfoArray);

                // 刷新图表
                lv_chart_refresh(chart);
                vPortFree(taskStatusArray);
            }
        }
    }
}

// 切换回主界面
void switch_to_main_screen(void)
{
    save_brightness_to_nvs(lv_slider_get_value(brightness_slider));
    lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 0, false);
}
void dropdown_screen_backbtn_cb(lv_event_t *e)
{
    switch_to_main_screen();
}