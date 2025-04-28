#include "UI.h"
#include "mpu6050.h"

void page6_ta_timer_cb(lv_timer_t *);
void page6_back_bnt(lv_event_t *);
void MPU6050_task(lv_timer_t *timer);
TaskHandle_t MPU6050_task_handel;
static lv_obj_t *chart;
static lv_chart_series_t *ser;
static lv_chart_series_t *ser2;
static lv_chart_series_t *ser3;
static lv_obj_t *ta_page6;
static lv_obj_t *label1;
lv_timer_t *mpu6050_page_timer;
int16_t i = 0;

extern lv_obj_t *main_screen;
static mpu6050_handle_t mpu6050;
static const char *TAG = "MPU6050";
void create_mpu_page()
{

    lv_obj_t *mpu6050_page = lv_obj_create(NULL);

    lv_obj_t *btn = lv_btn_create(mpu6050_page);
    lv_obj_set_size(btn, LV_PCT(100), 20);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t *img = lv_img_create(btn);
    lv_img_set_src(img, LV_SYMBOL_NEW_LINE);
    lv_obj_align(img, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "BACK");
    lv_obj_align_to(label, img, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(btn, page6_back_bnt, LV_EVENT_CLICKED, NULL);

    ta_page6 = lv_textarea_create(mpu6050_page);
    lv_obj_align(ta_page6, LV_ALIGN_TOP_MID, 0, 30);
    lv_textarea_set_placeholder_text(ta_page6, "log");
    lv_obj_set_size(ta_page6, LV_PCT(90), LV_PCT(30));

    label1 = lv_label_create(mpu6050_page);
    lv_obj_align_to(label1, ta_page6, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);

    chart = lv_chart_create(mpu6050_page);
    lv_obj_set_size(chart, LV_PCT(90), LV_PCT(30));
    lv_obj_align_to(chart, label1, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 50);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -90, 90);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_point_count(chart, 60);
    /*Do not display points on the data*/
    lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);

    ser = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
    ser2 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    ser3 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

    mpu6050 = mpu6050_create(I2C_NUM_0, MPU6050_I2C_ADDRESS);
    if (mpu6050 == NULL)
    {
        ESP_LOGE(TAG, "MPU6050 create failed");
        return;
    }
    esp_err_t ret = mpu6050_config(mpu6050, ACCE_FS_2G, GYRO_FS_250DPS);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "MPU6050 config failed");
        return;
    }
    ret = mpu6050_wake_up(mpu6050);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "MPU6050 wake-up failed");
        return;
    }
    ESP_LOGI(TAG, "MPU6050 initialized successfully");

    if (MPU6050_task_handel != NULL)
    {
        vTaskDelete(MPU6050_task_handel);
        MPU6050_task_handel = NULL;
    }

    mpu6050_page_timer = lv_timer_create(MPU6050_task, 30, NULL);
    lv_timer_set_repeat_count(mpu6050_page_timer, -1);
    lv_disp_load_scr(mpu6050_page);

    // lv_disp_set_rotation(disp_drv, 90);
}
void MPU6050_task(lv_timer_t *timer)
{
    mpu6050_acce_value_t acce;
    mpu6050_gyro_value_t gyro;
    mpu6050_temp_value_t temp;
    if (i > 100)
    {
        char buffer[128];
        mpu6050_get_temp(mpu6050, &temp);
        mpu6050_get_acce(mpu6050, &acce);
        mpu6050_get_gyro(mpu6050, &gyro);
        sprintf(buffer, "TEMPERATURE:%7.2f\nELERO X:%7.2f Y:%7.2f\nGYRO X:%7.2f Y:%7.2f Z:%7.2f\n",
                temp.temp, acce.acce_x, acce.acce_y, gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);
        lv_label_set_text(label1, buffer);
        i = 0;
    }
    i++;

    mpu6050_get_temp(mpu6050, &temp);
    mpu6050_get_acce(mpu6050, &acce);
    mpu6050_get_gyro(mpu6050, &gyro);

    lv_chart_set_next_value(chart, ser, gyro.gyro_x);
    lv_chart_set_next_value(chart, ser2, gyro.gyro_y);
    lv_chart_set_next_value(chart, ser3, gyro.gyro_z);
}

void page6_back_bnt(lv_event_t *e)
{
    lv_obj_t *old_page = lv_scr_act(); // 获得当前活动屏幕
    if (MPU6050_task_handel != NULL)
    {
        vTaskDelete(MPU6050_task_handel);
        MPU6050_task_handel = NULL;
    }
    mpu6050_sleep(mpu6050);
    mpu6050_delete(mpu6050);
    lv_timer_del(mpu6050_page_timer);
    lv_disp_load_scr(main_screen);
    lv_obj_del(old_page); // 新屏幕已加载，删除旧屏幕
}
