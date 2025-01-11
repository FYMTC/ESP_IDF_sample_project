#include <stdio.h>
#include "esp32_s3_main.h"

static const char *TAG = "MAIN";
QueueHandle_t gpio_evt_queue = NULL;
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "START");
    /**/
    // 配置 GPIO 引脚
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << SD_DET_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE};
    gpio_config(&io_conf);

    // 创建 GPIO 事件队列
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (gpio_evt_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // 创建 GPIO 任务
    ESP_LOGI(TAG, "xTaskCreate gpio_task");
    xTaskCreate(gpio_task, "gpio_task", 1024*4, NULL, 1, NULL);

    // 安装 GPIO 中断服务
    gpio_install_isr_service(0);
    gpio_isr_handler_add(SD_DET_PIN, gpio_isr_handler, (void *)SD_DET_PIN);

    // 上电时检查 SD_DET_PIN 状态
    bool sd_detected = gpio_get_level(SD_DET_PIN) == 0; // 低电平表示 SD 卡插入
    if (sd_detected)
    {
        ESP_LOGI(TAG, "SD card detected on boot");
        gpio_isr_handler((void *)SD_DET_PIN); // 模拟 GPIO 中断事件
    }

    // 初始化 I2C
    i2c_master_init(I2C_MASTER_NUM_0, I2C_MASTER_SDA_IO_0, I2C_MASTER_SCL_IO_0);
    i2c_master_init(I2C_MASTER_NUM_1, I2C_MASTER_SDA_IO_1, I2C_MASTER_SCL_IO_1);
    i2c_scan(I2C_MASTER_NUM_0);
    i2c_scan(I2C_MASTER_NUM_1);
/**/
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    lv_demo_benchmark();
    //lv_demo_stress();
    //lv_demo_music();
    //lv_demo_widgets();

    start_lvgl_task();

    //  while (1)
    // {  
    //     lv_timer_handler();
    //     vTaskDelay(pdMS_TO_TICKS(10));
    // }
}
