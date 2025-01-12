#include <stdio.h>
#include "esp32_s3_main.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "START");

    // 初始化 SD 卡
    sdcardinit(); 
    // 初始化 I2C
    i2c_master_init(I2C_MASTER_NUM_0, I2C_MASTER_SDA_IO_0, I2C_MASTER_SCL_IO_0);
    i2c_master_init(I2C_MASTER_NUM_1, I2C_MASTER_SDA_IO_1, I2C_MASTER_SCL_IO_1);
    i2c_scan(I2C_MASTER_NUM_0);
    i2c_scan(I2C_MASTER_NUM_1);
    // 初始化 lvgl，屏幕，触摸
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    //lvgl demos
    //lv_demo_benchmark();
    //lv_demo_stress();
    //lv_demo_music();
    //lv_demo_widgets();

    //lvgl games
    cube_game_start();
    //ballgame_start();
    //fly_game_start();
    //pvz_start();

    //start_lvgl_task();

     while (1)
    {  
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
