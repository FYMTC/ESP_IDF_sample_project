#pragma once
#include "lvgl.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <ctype.h>
void create_file_browser_ui();
