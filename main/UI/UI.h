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

void create_menu();
int get_brightness_from_nvs();
esp_err_t init_nvs();
void print_ram_info();