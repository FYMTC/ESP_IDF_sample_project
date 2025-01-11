#include <stdio.h>
#include "esp32_s3_main.h"

static const char *TAG = "main";
static bool spi_initialized = false; // 标记 SPI 总线是否已初始化
sdmmc_card_t *card = NULL;

/* Display */
LGFX_tft tft;
static void disp_init(void);
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
/* Touch pad */
CTP ctp;
static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
lv_indev_t *indev_touchpad;
lv_indev_t *indev_mouse;
lv_indev_t *indev_keypad;
lv_indev_t *indev_encoder;
lv_indev_t *indev_button;

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
    slot_config.host_id = SPI2_HOST; // 使用 SPI2 主机

    // 配置挂载参数
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    // 挂载 SD 卡
    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "SD card mounted successfully");
        // 输出 SD 卡基本信息
        if (card)
        {
            ESP_LOGI(TAG, "SD card information:");
            ESP_LOGI(TAG, "  Manufacturer ID: 0x%02X", card->cid.mfg_id);
            ESP_LOGI(TAG, "  OEM/Application ID: %d", card->cid.oem_id);
            ESP_LOGI(TAG, "  Product Name: %s", card->cid.name);
            ESP_LOGI(TAG, "  Product Revision: %d.%d",
                     card->cid.revision, card->cid.revision);
            ESP_LOGI(TAG, "  Serial Number: %d", card->cid.serial);

            uint8_t mfg_month = (card->cid.date >> 8) & 0x0F;  // 生产月份
            uint8_t mfg_year = (card->cid.date & 0xFF) + 2000; // 生产年份
            ESP_LOGI(TAG, "  Manufacture Date: %d/%d", mfg_month, mfg_year);

            ESP_LOGI(TAG, "  Card Size: %llu MB",
                     ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
            ESP_LOGI(TAG, "  Sector Size: %d bytes", card->csd.sector_size);
            ESP_LOGI(TAG, "  Max Frequency: %d kHz", card->max_freq_khz);
            ESP_LOGI(TAG, "  Real Frequency: %d kHz", card->real_freq_khz);
            ESP_LOGI(TAG, "  Relative Card Address (RCA): 0x%04X", card->rca);
            if (card->is_mem)
            {
                ESP_LOGI(TAG, "  Card Type: Memory Card");
            }
            else if (card->is_sdio)
            {
                ESP_LOGI(TAG, "  Card Type: SDIO Card");
            }
            else if (card->is_mmc)
            {
                ESP_LOGI(TAG, "  Card Type: MMC Card");
            }
            else
            {
                ESP_LOGI(TAG, "  Card Type: Unknown");
            }
            ESP_LOGI(TAG, "  Number of IO Functions: %d", card->num_io_functions);
            ESP_LOGI(TAG, "  Bus Width: %d-bit", 1 << card->log_bus_width);
            ESP_LOGI(TAG, "  Supports DDR Mode: %s", card->is_ddr ? "Yes" : "No");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }
}

void unmount_sd_card()
{
    /*if (card)
    {
        esp_vfs_fat_sdcard_unmount("/sdcard", card); // 使用挂载路径和 card 指针
        card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }*/
    if (card)
    {
        esp_vfs_fat_sdcard_unmount("/sdcard", card);
        card = NULL;
        ESP_LOGI(TAG, "SD card unmounted");
    }
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

/**********************    SD卡 ↑  *********************/
/**********************************************************/

/***********************************************************/
/**********************    lvgl ↓   *********************/
void lv_port_indev_init(void)
{
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    static lv_indev_drv_t indev_drv;
    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}
/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /*Your code comes here*/
    ctp.begin(I2C_NUM_1);
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    // static lv_coord_t last_x = 0;
    // static lv_coord_t last_y = 0;

    static int x_pos = 0;
    static int y_pos = 0;

    /*Save the pressed coordinates and the state*/
    // if(touchpad_is_pressed()) {
    //     touchpad_get_xy(&last_x, &last_y);
    //     data->state = LV_INDEV_STATE_PR;
    // }
    // else {
    //     data->state = LV_INDEV_STATE_REL;
    // }

    if (ctp.is_touched())
    {
        ctp.get_touch_pos(&x_pos, &y_pos);
        // Serial.printf("X:%d Y:%d\n", x_pos, y_pos);
        data->point.x = x_pos;
        data->point.y = y_pos;
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    // data->point.x = last_x;
    // data->point.y = last_y;
}

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    /**
     * LVGL requires a buffer where it internally draws the widgets.
     * Later this buffer will passed to your display driver's `flush_cb` to copy its content to your display.
     * The buffer has to be greater than 1 display row
     *
     * There are 3 buffering configurations:
     * 1. Create ONE buffer:
     *      LVGL will draw the display's content here and writes it to your display
     *
     * 2. Create TWO buffer:
     *      LVGL will draw the display's content to a buffer and writes it your display.
     *      You should use DMA to write the buffer's content to the display.
     *      It will enable LVGL to draw the next part of the screen to the other buffer while
     *      the data is being sent form the first buffer. It makes rendering and flushing parallel.
     *
     * 3. Double buffering
     *      Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
     *      This way LVGL will always provide the whole rendered screen in `flush_cb`
     *      and you only need to change the frame buffer's address.
     */

    /* Example for 1) */
    // static lv_disp_draw_buf_t draw_buf_dsc_1;
    // static lv_color_t buf_1[MY_DISP_HOR_RES * 120];                          /*A buffer for 10 rows*/
    // lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * MY_DISP_VER_RES);   /*Initialize the display buffer*/

    #if use_buf_dsc_2 

    /* Example for 2) */
    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 24];                        /*A buffer for 10 rows*/
    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 24];                        /*An other buffer for 10 rows*/
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 24);   /*Initialize the display buffer*/

    /* Example for 3) also set disp_drv.full_refresh = 1 below*/
    #endif

    
#if use_buf_dsc_3
    static lv_disp_draw_buf_t draw_buf_dsc_3;
    #if USE_PSRAM_FOR_BUFFER    /* Try to get buffer from PSRAM */
        static lv_color_t *buf_3_1 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);/*A screen sized buffer*/
        static lv_color_t *buf_3_2 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);/*Another screen sized buffer*/

        if ((buf_3_1 == NULL) || (buf_3_2 == NULL))
        {
            ESP_LOGE(TAG, "malloc buffer from PSRAM fialed");
            while (1)
                ;
        }
        else
        {
            ESP_LOGI(TAG, "malloc buffer from PSRAM successful");
        }
    #else
        //static lv_color_t *buf_3_1 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        //static lv_color_t *buf_3_2 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        static lv_color_t *buf_3_1 = (lv_color_t *)malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t)/2);
        static lv_color_t *buf_3_2 = (lv_color_t *)malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t)/2);
    #endif   
    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
                          MY_DISP_HOR_RES * MY_DISP_VER_RES/2); /*Initialize the display buffer*/

#endif
    // 获取内部 RAM 的内存信息
    size_t free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL)/1024;
    size_t total_size = heap_caps_get_total_size(MALLOC_CAP_INTERNAL)/1024;
    size_t used_size = total_size - free_size;

    ESP_LOGI(TAG, "Internal RAM:");
    ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
    ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
    ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);

    // 获取 PSRAM 的内存信息（如果可用）
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 0) {
        free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024;
        total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM)/1024;
        used_size = total_size - free_size;

        ESP_LOGI(TAG, "PSRAM:");
        ESP_LOGI(TAG, "  Total size: %d kbytes", total_size);
        ESP_LOGI(TAG, "  Used size: %d kbytes", used_size);
        ESP_LOGI(TAG, "  Free size: %d kbytes", free_size);
    } else {
        ESP_LOGI(TAG, "PSRAM not available");
    }

    

    // static lv_disp_draw_buf_t draw_buf_dsc_2;
    // static lv_color_t buf_2_1[MY_DISP_HOR_RES * 24];                        /*A buffer for 10 rows*/
    // static lv_color_t buf_2_2[MY_DISP_HOR_RES * 24];                        /*An other buffer for 10 rows*/
    // lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 24);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv; /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);   /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = MY_DISP_HOR_RES;//width
    disp_drv.ver_res = MY_DISP_VER_RES;//height

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    // disp_drv.draw_buf = &draw_buf_dsc_1;
    #if use_buf_dsc_2
    disp_drv.draw_buf = &draw_buf_dsc_2;
    #endif
    #if use_buf_dsc_3
    disp_drv.draw_buf = &draw_buf_dsc_3;
    #endif
    /*Required for Example 3)*/

    disp_drv.full_refresh = 1; // 全屏幕刷新
    /* Set LVGL software rotation */
    //disp_drv.sw_rotate = 1;//软件旋转屏幕
    // disp_drv.rotated = LV_DISP_ROT_90;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    // disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
    ESP_LOGI(TAG, "register the driver");
}

static void disp_init(void)
{
#if USE_LGFX == 1
    ESP_LOGI(TAG, "[LVGL] LGFX lcd init...");
    tft.init();
    // tft.setRotation(1);
    tft.setBrightness(32);
    // tft.fillScreen(TFT_RED);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.fillScreen(TFT_GREEN);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.fillScreen(TFT_BLUE);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    tft.fillScreen(TFT_BLACK);
#endif

#if USE_eTFT == 1
    Serial.println("[LVGL] eTFT lcd init...");
    tft = TFT_eSPI();
    tft.init();
    // tft.setRotation(2);
    tft.fillScreen(0xAD75);
#endif
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    // if(disp_flush_enabled) {
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

    // int32_t x;
    // int32_t y;
    // for(y = area->y1; y <= area->y2; y++) {
    //     for(x = area->x1; x <= area->x2; x++) {
    //         /*Put a pixel to the display. For example:*/
    //         /*put_px(x, y, *color_p)*/
    //         color_p++;
    //     }
    // }

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if USE_LGFX == 1
    if (tft.getStartCount() == 0)
    {
        tft.startWrite();

        tft.setAddrWindow(area->x1, area->y1, w, h);
        tft.pushPixels((uint16_t *)&color_p->full, w * h, true);
        //tft.pushColors((uint16_t *)&color_p->full, w * h, true);  // 非 DMA 传输
        //tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
        // tft.waitDMA();
        tft.endWrite();
    }

#endif

#if USE_eTFT == 1
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)&color_p->full, w * h);
    tft.endWrite();
#endif

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}

/**/
// LVGL 刷新任务
void lvgl_task(void *pvParameter) {
    while (1) {
        lv_timer_handler();     // 刷新 LVGL
        vTaskDelay(pdMS_TO_TICKS(10));  // 每 5ms 调用一次
    }
}

// 初始化 LVGL 刷新任务
void start_lvgl_task() {
    xTaskCreatePinnedToCore(lvgl_task, "LVGL Task", 4096, NULL, 1, NULL, 1);
}

    

/**********************    lvgl ↑  *********************/
/**********************************************************/