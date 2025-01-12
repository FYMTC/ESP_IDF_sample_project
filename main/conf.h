#pragma once

/******************************************************************************/
/***************************  I2C ↓ *******************************************/

#define I2C_MASTER_SCL_IO_0    GPIO_NUM_1   // GPIO for SCL (Bus 0)
#define I2C_MASTER_SDA_IO_0    GPIO_NUM_2    // GPIO for SDA (Bus 0)
#define I2C_MASTER_SCL_IO_1    GPIO_NUM_12    // GPIO for SCL (Bus 1)
#define I2C_MASTER_SDA_IO_1    GPIO_NUM_11    // GPIO for SDA (Bus 1)
#define I2C_MASTER_NUM_0       I2C_NUM_0
#define I2C_MASTER_NUM_1       I2C_NUM_1
#define I2C_MASTER_FREQ_HZ     100000

/***********************************************************/
/**********************    SD卡 ↓   *********************/
#define SD_DET_PIN   GPIO_NUM_17
#define SD_MISO_PIN  GPIO_NUM_16
#define SD_MOSI_PIN  GPIO_NUM_6
#define SD_CLK_PIN   GPIO_NUM_15
#define SD_CS_PIN    GPIO_NUM_5

#define SDCARD_SPIHOST SPI2_HOST


/******************************************************************************/
/***************************  LVGL gamed ↓ *************************************/
#define  cube_pre_x   100
#define  cube_pre_y   10
#define  cube_win_x   100
#define  cube_win_y   9
#define  cube_size    12

/******************************************************************************/
/***************************  scream ↓ ************************************/
#define MY_DISP_HOR_RES    240 //width
#define MY_DISP_VER_RES    280 //height

#define USE_LGFX 1
#define USE_eTFT 0

#define USE_PSRAM_FOR_BUFFER 0
#define use_buf_dsc_2 0 //fps34
#define use_buf_dsc_3 1 //fps45
