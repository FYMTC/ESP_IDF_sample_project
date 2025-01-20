#include <stdio.h>
#include "esp32_s3_main.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "START");

    // 初始化 SD 卡
    sdcardinit();
    //init_spiffs();
    // 初始化 NVS
    init_nvs();
    
    // 初始化 I2C
    i2c_master_init(I2C_MASTER_NUM_0, I2C_MASTER_SDA_IO_0, I2C_MASTER_SCL_IO_0);
    i2c_master_init(I2C_MASTER_NUM_1, I2C_MASTER_SDA_IO_1, I2C_MASTER_SCL_IO_1);
    i2c_scan(I2C_MASTER_NUM_0);
    i2c_scan(I2C_MASTER_NUM_1);

    // 初始化 lvgl，屏幕，触摸
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    
    start_info_task();

    wifi_service_init(); // 初始化wifi服务
    //设置省电模式
    //WIFI_PS_NONE,        /**< No power save */
    //WIFI_PS_MIN_MODEM,   /**< Minimum modem power saving. In this mode, station wakes up to receive beacon every DTIM period */
    //WIFI_PS_MAX_MODEM,   /**< Maximum modem power saving. In this mode, interval to receive beacons is determined by the listen_interval parameter in wifi_sta_config_t */
    wifi_service_set_power_save_mode(WIFI_PS_MIN_MODEM);
    
    //  lvgl demos
    //lv_demo_benchmark();
    //  lv_demo_stress();
    //  lv_demo_music();
    //  lv_demo_widgets();

    // lvgl games
    // cube_game_start();
    // ballgame_start();
    // fly_game_start();
    // pvz_start();
#if 1

    create_menu();

    bt_host_start();//蓝牙鼠标

    hid_host_main();

    initialize_sntp();

    //htpp_task();
#else
    // A:/sdcard/1.jpg
    // 2.png
    // R.jpg
    // lv_obj_t *img = lv_img_create(lv_scr_act());
    // lv_img_set_src(img, "/:/1.jpg");

    // lv_fs_file_t f;
    // lv_fs_res_t res = lv_fs_open(&f, "S:/image.bmp", LV_FS_MODE_RD);
    // if (res != LV_FS_RES_OK) {
    //     ESP_LOGE(TAG, "Failed to open image from SD card");
    //     return;
    // }

    // lv_img_dsc_t img_dsc;
    // //lv_img_decoder_bmp_init();  // 初始化 BMP 解码器（如果是 BMP 文件）

    // res = lv_img_decoder_open(&f, &img_dsc, LV_IMG_CF_RAW, 0);
    // if (res != LV_FS_RES_OK) {
    //     ESP_LOGE(TAG, "Failed to decode image");
    //     return;
    // }
    // lv_img_set_src(img, &img_dsc);
    //lv_fs_close(&f);

    // lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    // const void *src = lv_img_get_src(img);
    // if (src == NULL)
    // {
    //     ESP_LOGE(TAG, "Failed to load image\n");
    // }
    // else
    // {
    //     printf("Image loaded successfully\n");
    // }
    // lv_img_set_src(img, src);

    // list_sd_files(sdcard_mount_point);

    audio_app_main();

#endif
    // start_lvgl_task();

    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
