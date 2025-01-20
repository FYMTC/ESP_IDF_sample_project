file(GLOB_RECURSE GAME_SRCS
    game/*.c
    game/*.cpp
)
set(GAME_INCS
    game/
)

file(GLOB_RECURSE UI_SRCS
    UI/*.c
    UI/*.cpp
)
set(UI_INCS
    UI/
)

file(GLOB_RECURSE HAL_LVGL_SRCS
    hal_lvgl/*.c
    hal_lvgl/*.cpp
)
set(HAL_LVGL_INCS
    hal_lvgl/
)

file(GLOB_RECURSE BT_HID_host_SRCS
    BT_HID_host/*.c
    BT_HID_host/*.cpp
)
set(BT_HID_host_INCS
    BT_HID_host/
)

file(GLOB_RECURSE TASKS_SRCS
    tasks/*.c
    tasks/*.cpp
)
set(TASKS_INCS
    tasks/
)
# 设置额外的组件目录
set(EXTRA_COMPONENT_DIRS ${PROJECT_SOURCE_DIR}/components)
#set(COMPONENTS driver)
#target_link_libraries(${COMPONENT_TARGET} INTERFACE driver)
# 注册组件并指定源文件、头文件和依赖的库
idf_component_register(
    SRCS "main.cpp" "ESP32_S3_main.cpp" ${GAME_SRCS} ${UI_SRCS} ${HAL_LVGL_SRCS} ${BT_HID_host_SRCS} ${TASKS_SRCS}
    INCLUDE_DIRS "." ${GAME_INCS} ${UI_INCS} ${HAL_LVGL_INCS} ${BT_HID_host_INCS} ${TASKS_INCS}
    REQUIRES fatfs lvgl esp_lcd spiffs nvs_flash LovyanGFX esp_hid usb vfs esp_http_client esp-tls# 指定依赖的库
    EMBED_TXTFILES baidu_com.pem
)

# 设置编译选项
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-const-variable
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-error=format-truncation
)

# 添加额外的包含路径
#target_include_directories(${COMPONENT_LIB} PRIVATE
#    ${PROJECT_SOURCE_DIR}/game
#    ${PROJECT_SOURCE_DIR}/UI
#    ${PROJECT_SOURCE_DIR}/hal_lvgl
#)
