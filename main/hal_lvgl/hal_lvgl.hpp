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
#include "conf.h"
#include <lvgl.h>
#include "CTP_driver.hpp"
#include "hal_disp.hpp"
#include "UI/UI.h"

#ifdef __cplusplus
extern "C" {
#endif


void lv_port_indev_init(void);
void lv_port_disp_init(void);
void start_lvgl_task(void);

#ifdef __cplusplus
}
#endif

