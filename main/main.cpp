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

    // 初始化 lvgl，屏幕，输入设备
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    
//    start_info_task();

    wifi_service_init(); 
    wifi_service_set_power_save_mode(WIFI_PS_MIN_MODEM); //设置WIFI省电模式
    
#if 1

    create_menu();

    bt_host_start();//蓝牙鼠标

    //hid_host_main();//USB鼠标

    initialize_sntp();

    uac_init();
    audio_player_init();

    led_task_init();
    
#else
#endif

    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
