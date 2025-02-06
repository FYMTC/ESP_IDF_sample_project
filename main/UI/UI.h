#pragma once
#include "lvgl.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <esp_sleep.h>

#include "hal_disp.hpp"

#include "game/cubegame/cubegame.h"
#include "game/brickbreaker_project/ballgame.h"
#include "game/airplane_shooting_project/flygame.h"
#include "game/pvz_project/pvz.h"

#include "file_browser/sdcard.hpp"
#include "tasks/wifi_task/wifi_service.h"

#include "hid_host.h"
#include "tasks/audio_task/usb_uac.h"
#include "tasks/audio_task/audio_player.h"

#include "esp32_s3_main.h"

void create_menu();
int get_brightness_from_nvs();
void init_nvs();

void save_brightness_to_nvs(int brightness);
void create_dropdown_screen(void);
bool load_switch_state(const char *key) ;

void create_time_page() ;

void htpp_weather(char *out_city, char *out_weather_text, char *out_temperature);
void initialize_sntp(void);

