file(GLOB_RECURSE GAME_SRCS
    game/*.c
    game/*.cpp
)
set(GAME_INCS
    game/
)

idf_component_register(SRCS "main.cpp" "ESP32_S3_main.cpp" ${GAME_SRCS}
                       INCLUDE_DIRS "." ${GAME_INCS}
                       )


target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-unused-const-variable
    -Wno-unused-variable
    -Wno-unused-function
)