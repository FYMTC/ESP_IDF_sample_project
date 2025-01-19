#include <stdio.h>
#include "esp32_s3_main.h"

static const char *TAG = "main";
static bool spi_initialized = false; // 标记 SPI 总线是否已初始化
sdmmc_card_t *card = NULL;
QueueHandle_t gpio_evt_queue = NULL;
TaskHandle_t GPIOtask_handle;

/******************************************************************************/
/*************************** 扫描挂载 I2C 设备  ↓ ******************************/

static mpu6050_handle_t mpu6050 = NULL;

void i2c_master_init(i2c_port_t i2c_num, gpio_num_t sda_io, gpio_num_t scl_io)
{
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io,
        .scl_io_num = scl_io,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = I2C_MASTER_FREQ_HZ},
        .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL};

    i2c_param_config(i2c_num, &i2c_conf);
    i2c_driver_install(i2c_num, I2C_MODE_MASTER, 0, 0, 0);
}
void i2c_sensor_mpu6050_init(i2c_port_t i2c_num)
{
    mpu6050 = mpu6050_create(i2c_num, MPU6050_I2C_ADDRESS);
    if (mpu6050 == NULL)
    {
        ESP_LOGE(TAG, "MPU6050 create failed");
        return;
    }
    esp_err_t ret = mpu6050_config(mpu6050, ACCE_FS_4G, GYRO_FS_500DPS);
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
}

void i2c_scan(i2c_port_t i2c_num)
{
    ESP_LOGI(TAG, "Scanning I2C devices on bus %d...", i2c_num);

    for (uint8_t address = 1; address < 127; address++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);

        esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {
            ESP_LOGI(TAG, "Found device at address 0x%02X on bus %d", address, i2c_num);

            if (address == MPU6050_I2C_ADDRESS)
            { // 如果有MPU6050
                i2c_sensor_mpu6050_init(i2c_num);
                // while (1) {
                mpu6050_acce_value_t acce;
                mpu6050_gyro_value_t gyro;
                mpu6050_temp_value_t temp;

                if (mpu6050_get_acce(mpu6050, &acce) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Accel: X=%.2f Y=%.2f Z=%.2f", acce.acce_x, acce.acce_y, acce.acce_z);
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to read accelerometer data");
                }

                if (mpu6050_get_gyro(mpu6050, &gyro) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Gyro: X=%.2f Y=%.2f Z=%.2f", gyro.gyro_x, gyro.gyro_y, gyro.gyro_z);
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to read gyroscope data");
                }

                if (mpu6050_get_temp(mpu6050, &temp) == ESP_OK)
                {
                    ESP_LOGI(TAG, "Temp: %.2f°C", temp.temp);
                }
                else
                {
                    ESP_LOGE(TAG, "Failed to read temperature data");
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
                //}
            }
        }
    }

    ESP_LOGI(TAG, "Scan complete on bus %d.", i2c_num);
}

/***************************  I2C ↑  *******************************************/
/*******************************************************************************/

/***********************************************************/
/**********************    SD卡 ↓   *********************/
void initialize_spi_bus()
{
    if (spi_initialized)
    {
        return; // 如果 SPI 总线已初始化，直接返回
    }

    // 配置 SPI 总线
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI_PIN, // GPIO_NUM_6
        .miso_io_num = SD_MISO_PIN, // GPIO_NUM_16
        .sclk_io_num = SD_CLK_PIN,  // GPIO_NUM_15
        .quadwp_io_num = -1,        // 不使用
        .quadhd_io_num = -1,        // 不使用
        .max_transfer_sz = 4000,    // 最大传输大小
    };

    // 初始化 SPI 总线
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return;
    }

    spi_initialized = true; // 标记 SPI 总线已初始化
    ESP_LOGI(TAG, "SPI bus initialized");
}

void deinitialize_spi_bus()
{
    if (spi_initialized)
    {
        spi_bus_free(SPI2_HOST);
        spi_initialized = false;
        ESP_LOGI(TAG, "SPI bus deinitialized");
    }
}

void mount_sd_card()
{

    if (!spi_initialized)
    {
        initialize_spi_bus();
    }

    // 配置 SDMMC 主机
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // 配置 SDSPI 设备
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN; // GPIO_NUM_5
    slot_config.host_id = SDCARD_SPIHOST; // 使用 SPI2 主机

    // 配置挂载参数
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // 挂载 SD 卡
    esp_err_t ret = esp_vfs_fat_sdspi_mount(sdcard_mount_point, &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SD card mounted successfully");

        if (card)
        {
            // 输出 SD 卡基本信息
            sdmmc_card_print_info(stdout, card);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }
}

void unmount_sd_card()
{
    deinitialize_spi_bus();
    if (card)
    {
        esp_vfs_fat_sdcard_unmount(sdcard_mount_point, card);
        card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }
}
void init_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = sdcard_mount_point,
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS partition size: total: %d, used: %d", total, used);
    }
}
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken) != pdPASS)
    {
        ESP_LOGW(TAG, "Failed to send data to queue");
    }
    // if (xHigherPriorityTaskWoken) {
    //     portYIELD_FROM_ISR();
    // }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void gpio_task(void *arg)
{
    uint32_t io_num;
    bool sd_inserted = false;

    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(1000)))
        {
            bool current_state = gpio_get_level(SD_DET_PIN) == 0;
            ESP_LOGI(TAG, "GPIO event received, state: %d", current_state);

            if (current_state && !sd_inserted)
            {
                ESP_LOGW(TAG, "SD card inserted");
                mount_sd_card();
                sd_inserted = true;
            }
            else if (!current_state && sd_inserted)
            {
                ESP_LOGW(TAG, "SD card removed");
                unmount_sd_card();
                sd_inserted = false;
            }
        }
        else
        {
            // ESP_LOGW(TAG, "Timeout waiting for GPIO event");
        }
    }
}

void sdcardinit(void){
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
    xTaskCreatePinnedToCore(gpio_task, "gpio_task", 1024*3, NULL, 1, &GPIOtask_handle, 1);

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
}

void list_sd_files(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        printf("Failed to open directory: %s\n", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}

/**********************    SD卡 ↑  *********************/
/**********************************************************/

/**/
// info 刷新任务
void info_task(void *pvParameter) {
    while (1) {
        print_ram_info();
        vTaskDelay(pdMS_TO_TICKS(10000));  // 每 10s 调用一次
    }
}

// 初始化 info 刷新任务
void start_info_task() {
    portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();
    xTaskCreatePinnedToCore(info_task, "tasks info Task", 1024*3, NULL, 1, NULL, 1);
}

    