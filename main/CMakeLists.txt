# 递归获取游戏、UI 和 LVGL 相关的源代码和头文件
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

# 设置额外的组件目录
set(EXTRA_COMPONENT_DIRS ${PROJECT_SOURCE_DIR}/components)
#set(COMPONENTS driver)
#target_link_libraries(${COMPONENT_TARGET} INTERFACE driver)
# 注册组件并指定源文件、头文件和依赖的库
idf_component_register(
    SRCS "main.cpp" "ESP32_S3_main.cpp" ${GAME_SRCS} ${UI_SRCS} ${HAL_LVGL_SRCS}
    INCLUDE_DIRS "." ${GAME_INCS} ${UI_INCS} ${HAL_LVGL_INCS}
    REQUIRES fatfs lvgl bt esp_lcd spiffs nvs_flash LovyanGFX esp_hid# 指定依赖的库
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
