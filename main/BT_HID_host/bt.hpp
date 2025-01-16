#pragma once
#include <stdio.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
void bt_host_start(void);
#ifdef __cplusplus
}
#endif

typedef struct {
    uint8_t button_state;  // 按钮状态
    int8_t x_movement;     // X 轴移动
    int8_t x_movement_direction;     // X 轴移动状态
    int8_t y_movement;     // Y 轴移动
    int8_t y_movement_direction;     // Y 轴移动状态
    int8_t wheel_movement; // 滚轮移动
    bool left_button_pressed;   // 左键是否按下
    bool right_button_pressed;  // 右键是否按下
    uint8_t data_frame; // 报告 ID
    bool mouse_connect_state;
} mouse_t;



