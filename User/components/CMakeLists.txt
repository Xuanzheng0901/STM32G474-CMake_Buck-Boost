file(GLOB_RECURSE COMPONENT_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/ADC/*.c
        ${CMAKE_CURRENT_LIST_DIR}/pid_ctrl/*.c
        ${CMAKE_CURRENT_LIST_DIR}/lvgl_port/*.c
        ${CMAKE_CURRENT_LIST_DIR}/stm_ssd1306/*.c
        ${CMAKE_CURRENT_LIST_DIR}/UI/*.c
)

add_library(user_components INTERFACE)
target_sources(user_components INTERFACE ${COMPONENT_SOURCES})

target_include_directories(user_components INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/ADC
        ${CMAKE_CURRENT_LIST_DIR}/pid_ctrl
        ${CMAKE_CURRENT_LIST_DIR}/lvgl_port
        ${CMAKE_CURRENT_LIST_DIR}/stm_ssd1306
        ${CMAKE_CURRENT_LIST_DIR}/UI
)