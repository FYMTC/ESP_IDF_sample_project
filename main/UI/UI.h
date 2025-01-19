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

#include "sdcard.hpp"
#include "tasks/wifi_task/wifi_service.h"

#include "hid_host.h"
void create_menu();
int get_brightness_from_nvs();
void init_nvs();
void print_ram_info();
LV_FONT_DECLARE(NotoSansSC_Medium_3500);
LV_FONT_DECLARE(my_font);


void save_brightness_to_nvs(int brightness);
void create_dropdown_screen(void);
bool load_switch_state(const char *key) ;

void create_time_page() ;
