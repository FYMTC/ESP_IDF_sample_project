#pragma once

#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"

#include "driver/spi_master.h"
#include "driver/i2s_std.h"

#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"// SDMMC host driver
#include "driver/sdspi_host.h"// SDSPI host driver
#include "driver/spi_common.h"

#include "driver/gpio.h"

#include "mpu6050.h"

#include <lvgl.h>
#include "disp/hal_disp.hpp"
#include "CTP_driver/CTP_driver.hpp"
#include "demos/lv_demos.h"

/******************************************************************************/
/***************************  I2C ↓ *******************************************/

#define I2C_MASTER_SCL_IO_0    GPIO_NUM_1   // GPIO for SCL (Bus 0)
#define I2C_MASTER_SDA_IO_0    GPIO_NUM_2    // GPIO for SDA (Bus 0)
#define I2C_MASTER_SCL_IO_1    GPIO_NUM_12    // GPIO for SCL (Bus 1)
#define I2C_MASTER_SDA_IO_1    GPIO_NUM_11    // GPIO for SDA (Bus 1)
#define I2C_MASTER_NUM_0       I2C_NUM_0
#define I2C_MASTER_NUM_1       I2C_NUM_1
#define I2C_MASTER_FREQ_HZ     100000

void i2c_master_init(i2c_port_t i2c_num, gpio_num_t sda_io, gpio_num_t scl_io);// 初始化I2C接口
void i2c_scan(i2c_port_t i2c_num); 

/***************************  I2C ↑  *******************************************/
/*******************************************************************************/


/***********************************************************/
/**********************    SD卡 ↓   *********************/
#define SD_DET_PIN   GPIO_NUM_17
#define SD_MISO_PIN  GPIO_NUM_16
#define SD_MOSI_PIN  GPIO_NUM_6
#define SD_CLK_PIN   GPIO_NUM_15
#define SD_CS_PIN    GPIO_NUM_5

extern QueueHandle_t gpio_evt_queue;


void gpio_task(void *arg);
void gpio_isr_handler(void *arg);



/**********************    SD卡 ↑  *********************/
/**********************************************************/


#define MY_DISP_HOR_RES    240 //width
#define MY_DISP_VER_RES    280 //height

#define USE_LGFX 1
#define USE_eTFT 0

#define USE_PSRAM_FOR_BUFFER 0
#define use_buf_dsc_2 1 //fps75
#define use_buf_dsc_3 0 //fps45

void lv_port_indev_init(void);
void lv_port_disp_init(void);
void start_lvgl_task(void);