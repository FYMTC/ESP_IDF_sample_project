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
#include "game/cubegame/cubegame.h"
#include "game/brickbreaker_project/ballgame.h"
#include "game/airplane_shooting_project/flygame.h"
#include "game/pvz_project/pvz.h"

#include "conf.h"

// /******************************************************************************/
// /***************************  I2C ↓ *******************************************/

void i2c_master_init(i2c_port_t i2c_num, gpio_num_t sda_io, gpio_num_t scl_io);// 初始化I2C接口
void i2c_scan(i2c_port_t i2c_num); 

/***************************  I2C ↑  *******************************************/
/*******************************************************************************/


/***********************************************************/
/**********************    SD卡 ↓   *********************/
void sdcardinit(void);


/**********************    SD卡 ↑  *********************/
/**********************************************************/

void lv_port_indev_init(void);
void lv_port_disp_init(void);
void start_lvgl_task(void);
