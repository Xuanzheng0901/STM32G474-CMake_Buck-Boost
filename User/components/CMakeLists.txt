file(GLOB_RECURSE COMPONENT_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/ADC/*.c
        ${CMAKE_CURRENT_LIST_DIR}/pid_ctrl/*.c
        ${CMAKE_CURRENT_LIST_DIR}/display/*.c
        ${CMAKE_CURRENT_LIST_DIR}/display/lvgl_port/*.c
        ${CMAKE_CURRENT_LIST_DIR}/display/stm_ssd1306/*.c
        ${CMAKE_CURRENT_LIST_DIR}/display/stm_sh1107/*.c
        ${CMAKE_CURRENT_LIST_DIR}/UI/*.c
        ${CMAKE_CURRENT_LIST_DIR}/LOG/*.c
        ${CMAKE_CURRENT_LIST_DIR}/filter/*.c
        ${CMAKE_CURRENT_LIST_DIR}/PLL/*.c
)

add_library(user_components INTERFACE)
target_sources(user_components INTERFACE ${COMPONENT_SOURCES})

target_include_directories(user_components INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/ADC
        ${CMAKE_CURRENT_LIST_DIR}/pid_ctrl
        ${CMAKE_CURRENT_LIST_DIR}/display
        ${CMAKE_CURRENT_LIST_DIR}/display/lvgl_port
        ${CMAKE_CURRENT_LIST_DIR}/display/stm_ssd1306
        ${CMAKE_CURRENT_LIST_DIR}/display/stm_sh1107
        ${CMAKE_CURRENT_LIST_DIR}/UI
        ${CMAKE_CURRENT_LIST_DIR}/LOG
        ${CMAKE_CURRENT_LIST_DIR}/filter
        ${CMAKE_CURRENT_LIST_DIR}/PLL/
)