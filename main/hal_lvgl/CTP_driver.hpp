#pragma once

#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* Hardware config */
#define CTP_DEV_ADDR 0x15
#define CTP_INT_PIN     49
/* Software config */
#define CTP_REVERS_XPOS 0
#define CTP_REVERS_YPOS 1
#define CTP_REVERS_XY   0

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_SDA_IO 11
#define I2C_MASTER_SCL_IO 12

class I2C_PORT {
    private:
        uint8_t _dev_addr;
        i2c_port_t _i2c_port;

    protected:
        void _I2C_init(i2c_port_t i2c_port, uint8_t dev_addr) {
            _i2c_port = i2c_port;
            _dev_addr = dev_addr;
        }

        bool _I2C_checkDevAvl() {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (_dev_addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
            return ret == ESP_OK;
        }

        void _I2C_write1Byte(uint8_t addr, uint8_t data) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (_dev_addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_write_byte(cmd, addr, true);
            i2c_master_write_byte(cmd, data, true);
            i2c_master_stop(cmd);
            i2c_master_cmd_begin(_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
        }

        uint8_t _I2C_read8Bit(uint8_t addr) {
            uint8_t data;
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (_dev_addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_write_byte(cmd, addr, true);
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (_dev_addr << 1) | I2C_MASTER_READ, true);
            i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
            i2c_master_stop(cmd);
            i2c_master_cmd_begin(_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
            return data;
        }

        void _I2C_readBuff(uint8_t addr, int size, uint8_t buff[]) {
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (_dev_addr << 1) | I2C_MASTER_WRITE, true);
            i2c_master_write_byte(cmd, addr, true);
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (_dev_addr << 1) | I2C_MASTER_READ, true);
            i2c_master_read(cmd, buff, size, I2C_MASTER_LAST_NACK);
            i2c_master_stop(cmd);
            i2c_master_cmd_begin(_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
            i2c_cmd_link_delete(cmd);
        }

    public:
        int I2C_dev_scan() {
            uint8_t address;
            int nDevices = 0;

            ESP_LOGI("I2C_SCAN", "device scanning...");

            for (address = 1; address < 127; address++) {
                i2c_cmd_handle_t cmd = i2c_cmd_link_create();
                i2c_master_start(cmd);
                i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
                i2c_master_stop(cmd);
                esp_err_t ret = i2c_master_cmd_begin(_i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
                i2c_cmd_link_delete(cmd);

                if (ret == ESP_OK) {
                    ESP_LOGI("I2C_SCAN", "device found at address 0x%02X", address);
                    nDevices++;
                } else if (ret == ESP_ERR_TIMEOUT) {
                    ESP_LOGI("I2C_SCAN", "timeout at address 0x%02X", address);
                }
            }

            ESP_LOGI("I2C_SCAN", "%d devices found", nDevices);
            return nDevices;
        }
};

class CTP : public I2C_PORT {
    private:
        int _x_pos;
        int _y_pos;

        void _init() {
            _reset_coor();
            _I2C_write1Byte(0xFE, 0xFF);
            ESP_LOGI("CTP", "auto sleep shut down");
        }

        void _reset_coor() {
            _x_pos = -1;
            _y_pos = -1;
        }

        void _update_coor() {
            uint8_t buff[4];
            _I2C_readBuff(0x03, 4, buff);

            #if CTP_REVERS_XY
                _y_pos = ((buff[0]&0x0F)<<8)|buff[1];
                _x_pos = ((buff[2]&0x0F)<<8)|buff[3];
                #if CTP_REVERS_XPOS
                    _x_pos = -(_x_pos ) + 280;
                #endif
                #if CTP_REVERS_YPOS
                    _y_pos = -(_y_pos ) + 240;
                #endif
            #else
                _x_pos = ((buff[0]&0x0F)<<8)|buff[1];
                _y_pos = ((buff[2]&0x0F)<<8)|buff[3];
            #endif
        }

    public:
        void begin(i2c_port_t i2c_port) {
            _I2C_init(i2c_port, CTP_DEV_ADDR);
            while (_I2C_checkDevAvl()) {
                ESP_LOGW("CTP", "please touch screen to activate it");
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
            ESP_LOGI("CTP", "init successful");
            _init();
        }

        bool is_touched() {
            return _I2C_read8Bit(0x02) ? true : false;
        }

        void get_touch_pos(int * x_pos, int * y_pos) {
            _update_coor();
            *x_pos = _x_pos;
            *y_pos = _y_pos;
        }

        void print_coordinate() {
            if (is_touched()) {
                _update_coor();
            } else {
                _reset_coor();
            }
            ESP_LOGI("CTP", "X:%d Y:%d", _x_pos, _y_pos);
        }
};

class GPIO_PORT {
    protected:
        void _GPIO_init(uint8_t pin) {
            gpio_config_t io_conf;
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << pin);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);
        }

        void _GPIO_write(uint8_t pin, uint8_t state) {
            gpio_set_level((gpio_num_t)pin, state);
        }

        void _GPIO_PWM_init(uint8_t pin, uint8_t channel) {
            ledc_timer_config_t timer_conf;
            timer_conf.duty_resolution = LEDC_TIMER_8_BIT;
            timer_conf.freq_hz = 5000;
            timer_conf.speed_mode = LEDC_LOW_SPEED_MODE;
            timer_conf.timer_num = LEDC_TIMER_0;
            ledc_timer_config(&timer_conf);

            ledc_channel_config_t ledc_conf;
            ledc_conf.channel = (ledc_channel_t)channel;
            ledc_conf.duty = 0;
            ledc_conf.gpio_num = (gpio_num_t)pin;
            ledc_conf.speed_mode = LEDC_LOW_SPEED_MODE;
            ledc_conf.timer_sel = LEDC_TIMER_0;
            ledc_channel_config(&ledc_conf);
        }

        void _GPIO_PWM_setDuty(uint8_t channel, uint8_t duty) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
        }
};

