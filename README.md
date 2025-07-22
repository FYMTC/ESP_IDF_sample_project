

ESP32-S3-WROOM-1-N16R8 16MB(QuadSPI) 8MB(OctalSPI)

idf.py add-dependency "lvgl/lvgl^8.3.7"
idf.py add-dependency "espressif/mpu6050^1.2.0"

gir clone https://github.com/lovyan03/LovyanGFX.git

对于1.69寸SPI位接口ST7789V 在lvgl设置成240*280 在LGFX 240*320 并在Y方向偏移20

DMA 和PSRAM 不能一起开

通过 menuconfig 启用配置项：IDF_EXPERIMENTAL_FEATURES SPIRAM_SPEED_120M SPIRAM_MODE_OCT 启用 PSRAM 120M 八进制 （DDR）
