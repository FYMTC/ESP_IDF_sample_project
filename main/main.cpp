#include <stdio.h>
#include "esp32_s3_main.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "START");

    // 初始化 SD 卡
    sdcardinit();
    // 初始化 NVS
    init_nvs();
    // 初始化 I2C
    i2c_master_init(I2C_NUM_0);
    i2c_scan(I2C_NUM_0);
    // 初始化 lvgl，屏幕，输入设备
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    start_info_task();

    wifi_service_init();
    wifi_service_set_power_save_mode(WIFI_PS_MIN_MODEM); // 设置WIFI省电模式
    initialize_sntp(); // 初始化SNTP
#if 1

    create_menu();

    //lv_demo_benchmark();

    bt_host_start(); // 蓝牙鼠标

    // hid_host_main();//USB鼠标

    //uac_init();//USB音频输出，打开后USB CDC不工作。 
    //audio_player_init();

    // led_task_init();
    brightness_task_main();

    //rtc_task();

#else
#endif

    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
