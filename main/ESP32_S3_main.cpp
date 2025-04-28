#include <stdio.h>
#include <dirent.h>
#include "esp32_s3_main.h"
#include "mpu6050.h"
#define class cls
#include "usb/uac_host.h"
#undef class

#define MPU6050_ADDR 0x68  // MPU6050的I2C地址
#define AXP2101_ADDR 0x34  // AXP2101的I2C地址
#define QMC5883_ADDR 0x0D  // QMC5883的I2C地址
#define NAU88C22_ADDR 0x1A // NAU88C22的I2C地址
#define PCA9554_ADDR 0x38  // PCA9554的I2C地址
#define CST128_ADDR 0x2A   // CST128的I2C地址

static mpu6050_handle_t mpu6050 = NULL;
static const char *TAG = "main";
static bool spi_initialized = false; // 标记 SPI 总线是否已初始化
sdmmc_card_t *card = NULL;
QueueHandle_t gpio_evt_queue = NULL;
TaskHandle_t GPIOtask_handle;
extern uac_host_device_handle_t s_spk_dev_handle;
// 任务信息结构体，用于排序
typedef struct
{
    char taskName[MAX_TASK_NAME_LEN];
    UBaseType_t highWaterMark;
    uint32_t cpuUsage;
} TaskInfo;

void i2c_master_init(i2c_port_t i2c_num)
{
    i2c_config_t conf;
    memset(&conf, 0, sizeof(conf));
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO_0;
    conf.scl_io_num = I2C_MASTER_SCL_IO_0;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    esp_err_t err = i2c_param_config(i2c_num, &conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(err));
        return;
    }
    err = i2c_driver_install(i2c_num, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "I2C master initialized successfully");
}
void read_mpu6050_data()
{
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

    // vTaskDelay(pdMS_TO_TICKS(1000));
    // }
}
void i2c_sensor_mpu6050_test(i2c_port_t i2c_num)
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
    read_mpu6050_data();
    mpu6050_sleep(mpu6050);
    mpu6050_delete(mpu6050);
}
void init_external_gpio()
{
    // pca9554_set_pin_mode(I2C_NUM_0, 0, 0);
    // pca9554_set_pin_mode(I2C_NUM_0, 1, 0);
    // pca9554_write_pin(I2C_NUM_0, 0, 0);
    // pca9554_write_pin(I2C_NUM_0, 1, 0);

    // pca9554_set_pin_mode(I2C_NUM_0, 7, 0);
    // pca9554_write_pin(I2C_NUM_0, 7, 0);
    // vTaskDelay(pdMS_TO_TICKS(10));
    // pca9554_write_pin(I2C_NUM_0, 7, 1);

    ESP_LOGI(TAG, "external_gpio initialized successfully");
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
        // ESP_LOGI(TAG,"I2C SCANING 0X%02X",address);
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK)
        {

            if (address == MPU6050_I2C_ADDRESS)
            { // 如果有MPU6050
                ESP_LOGI(TAG, "Found MPU6050 at address 0x%02X on bus %d", address, i2c_num);
                //i2c_sensor_mpu6050_test(i2c_num);
            }
            else if (address == AXP2101_ADDR)
            {
                ESP_LOGI(TAG, "Found AXP2101 at address 0x%02X on bus %d", address, i2c_num);
                pmu_init(); // 初始化电源管理芯片
            }
            else if (address == PCA9554_ADDR)
            {
                ESP_LOGI(TAG, "Found PCA9554 at address 0x%02X on bus %d", address, i2c_num);
                init_external_gpio();
            }
            else if(address == CST128_ADDR)
            {
                ESP_LOGI(TAG, "Found CST128 at address 0x%02X on bus %d", address, i2c_num);
                // cst128_dev_t cst128_dev = {
                //     .i2c_port = i2c_num,
                //     .rst_pin = PCA9554_PORT_7,
                //     .int_pin = PCA9554_PORT_6,
                //     .rst_valid = 0,
                //     .range_x = 240,
                //     .range_y = 320,
                // };
                // cst128_init(&cst128_dev);
            }
            else
            {
                ESP_LOGI(TAG, "Found unknow device at address 0x%02X on bus %d", address, i2c_num);
            }
        }
    }

    ESP_LOGI(TAG, "Scan complete on bus %d.", i2c_num);
}
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
        .flags = 0,                 // 默认值
        .intr_flags = 0             // 默认值
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
#if SD_USE_MMC_HOST
    esp_err_t ret;

    // 配置SD/MMC主机
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();

    // 自定义SD/MMC插槽引脚配置
    sdmmc_slot_config_t slot_config = {
        .clk = SDMMC_CLK_GPIO,  // CLK信号引脚
        .cmd = SDMMC_CMD_GPIO,  // CMD信号引脚
        .d0 = SDMMC_DATA0_GPIO, // D0信号引脚
        .d1 = SDMMC_DATA1_GPIO, // D1信号引脚 (4线模式)
        .d2 = SDMMC_DATA2_GPIO, // D2信号引脚 (4线模式)
        .d3 = SDMMC_DATA3_GPIO, // D3信号引脚 (4线模式)
        .cd = SD_DET_PIN,       // 卡检测引脚
        .wp = SDMMC_SLOT_NO_WP, // 不使用写保护引脚
        .width = 4,             // 总线宽度 (1或4)
        .flags = 0,             // 额外标志
    };

    // 挂载文件系统
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdmmc_mount(sdcard_mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }

    // 打印SD/MMC卡信息
    sdmmc_card_print_info(stdout, card);
#else
    if (!spi_initialized)
    {
        initialize_spi_bus();
    }

    // 配置 SDMMC 主机
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // 配置 SDSPI 设备
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS_PIN;      // GPIO_NUM_5
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

#endif
}

void unmount_sd_card()
{
#if SD_USE_MMC_HOST
    // 卸载SD/MMC卡
    esp_vfs_fat_sdcard_unmount(sdcard_mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");
#else
    deinitialize_spi_bus();
    if (card)
    {
        esp_vfs_fat_sdcard_unmount(sdcard_mount_point, card);
        card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }
#endif
}
void init_spiffs()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = sdcard_mount_point,
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
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
void init_nvs()
{
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Erasing NVS flash...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
}
void sdcardinit(void)
{
    // 配置 SD_DET_PIN 检测引脚
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
    xTaskCreatePinnedToCore(gpio_task, "gpio_task", 1024 * 3, NULL, 1, &GPIOtask_handle, 1);

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

void list_sd_files(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        printf("Failed to open directory: %s\n", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        printf("%s\n", entry->d_name);
    }

    closedir(dir);
}

/**********************    SD卡 ↑  *********************/
/**********************************************************/

// 输出内存状态信息
int compare_tasks_info(const void *a, const void *b)
{
    TaskInfo *taskA = (TaskInfo *)a;
    TaskInfo *taskB = (TaskInfo *)b;
    return taskB->highWaterMark - taskA->highWaterMark;
}
void print_ram_info()
{
    // 获取内部 RAM 的内存信息
    size_t free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024;
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL) / 1024;
    size_t used_size = total_size - free_size;

    ESP_LOGI(TAG, "Internal RAM:");
    ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
    ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
    ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);

    // 获取 PSRAM 的内存信息（如果可用）
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0)
    {
        free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024;
        total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024;
        used_size = total_size - free_size;

        ESP_LOGI(TAG, "PSRAM:");
        ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
        ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
        ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);
    }
    else
    {
        ESP_LOGI(TAG, "PSRAM not available");
    }

    TaskStatus_t *taskStatusArray = NULL;
    UBaseType_t taskCount, index;
    uint32_t totalRunTime;

    // 获取当前任务数量并保存
    UBaseType_t originalTaskCount = uxTaskGetNumberOfTasks();

    // 分配内存来存储任务状态
    taskStatusArray = (TaskStatus_t *)pvPortMalloc(originalTaskCount * sizeof(TaskStatus_t));

    if (taskStatusArray != NULL)
    {
        // 获取任务状态信息
        taskCount = uxTaskGetSystemState(taskStatusArray, originalTaskCount, &totalRunTime);

        if (taskCount > 0)
        {
            // 分配内存来存储排序后的任务信息
            TaskInfo *taskInfoArray = (TaskInfo *)pvPortMalloc(originalTaskCount * sizeof(TaskInfo));
            if (taskInfoArray == NULL)
            {
                ESP_LOGE(TAG, "Failed to allocate task info array");
                vPortFree(taskStatusArray);
                return;
            }

            // 打印表头
            printf("Task Name\t\tHigh Water Mark\tCPU Usage\n");
            printf("--------------------------------------------\n");

            // 打印每个任务的高水位线和 CPU 占用率
            for (index = 0; index < originalTaskCount; index++) // 使用 originalTaskCount
            {
                // 获取任务的栈高水位线
                taskInfoArray[index].highWaterMark = uxTaskGetStackHighWaterMark(taskStatusArray[index].xHandle);

                // 计算 CPU 占用率
                taskInfoArray[index].cpuUsage = 0;
                if (totalRunTime > 0)
                {
                    taskInfoArray[index].cpuUsage = (taskStatusArray[index].ulRunTimeCounter * 100) / totalRunTime;
                }

                // 安全复制任务名
                strncpy(taskInfoArray[index].taskName, taskStatusArray[index].pcTaskName, MAX_TASK_NAME_LEN - 1);
                taskInfoArray[index].taskName[MAX_TASK_NAME_LEN - 1] = '\0';
            }

            // 对任务信息进行排序
            qsort(taskInfoArray, originalTaskCount, sizeof(TaskInfo), compare_tasks_info);

            // 打印排序后的任务信息
            for (index = 0; index < originalTaskCount; index++)
            {
                printf("%-16s\t%u\t\t%lu%%\n",
                       taskInfoArray[index].taskName,
                       taskInfoArray[index].highWaterMark,
                       taskInfoArray[index].cpuUsage);
            }

            // 释放任务信息数组内存
            vPortFree(taskInfoArray);
        }
        else
        {
            printf("Failed to get task state information\n");
        }

        // 释放任务状态数组内存
        vPortFree(taskStatusArray);
    }
    else
    {
        printf("Failed to allocate memory for task status array\n");
    }
}
// info 刷新任务
void info_task(void *pvParameter)
{
    while (1)
    {
        print_ram_info();
        vTaskDelay(pdMS_TO_TICKS(10000)); // 每 10s 调用一次
    }
}

// 初始化 info 刷新任务
void start_info_task()
{
    portCONFIGURE_TIMER_FOR_RUN_TIME_STATS();
    xTaskCreatePinnedToCore(info_task, "tasks info Task", 1024 * 3, NULL, 1, NULL, 1);
}

uint8_t get_volume_from_nvs()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for reading!");
        return 50;
    }

    uint8_t volume = 50;
    err = nvs_get_u8(my_handle, BRIGHTNESS_KEY, &volume);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read brightness from NVS, using default.");
        volume = 50;
    }

    nvs_close(my_handle);

    return volume;
}
// 读取 NVS 中的亮度值
int get_brightness_from_nvs()
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for reading!");
        save_brightness_to_nvs(50); // 保存默认亮度值 50
        return 50;                  // 默认亮度值 50
    }

    int32_t brightness = 50; // 默认亮度
    err = nvs_get_i32(my_handle, BRIGHTNESS_KEY, &brightness);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to read brightness from NVS, using default.");
        brightness = 50;                    // 默认亮度
        save_brightness_to_nvs(brightness); // 保存默认亮度值
    }

    nvs_close(my_handle);
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

void save_volume_to_nvs(uint8_t volume)
{
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle for saving!");
        return;
    }

    err = nvs_set_u8(my_handle, BRIGHTNESS_KEY, volume);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write volume to NVS.");
    }
    else
    {
        ESP_LOGI(TAG, "volume saved to NVS: %d", volume);
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

void set_uac_volume(uint8_t volume)
{
    uac_host_device_set_volume(s_spk_dev_handle, volume);
}