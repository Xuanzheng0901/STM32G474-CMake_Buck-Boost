# Collect all component source files
file(GLOB_RECURSE COMPONENT_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/ADC/*.c
        ${CMAKE_CURRENT_LIST_DIR}/pid_ctrl/*.c
        ${CMAKE_CURRENT_LIST_DIR}/lvgl_port/*.c
        ${CMAKE_CURRENT_LIST_DIR}/stm_ssd1306/*.c
        ${CMAKE_CURRENT_LIST_DIR}/UI/*.c
)

# Set component sources to parent scope so main CMakeLists.txt can use them
set(COMPONENT_SOURCES ${COMPONENT_SOURCES} PARENT_SCOPE)

# Collect all component include directories
set(COMPONENT_INCLUDES
        ${CMAKE_CURRENT_LIST_DIR}/ADC
        ${CMAKE_CURRENT_LIST_DIR}/pid_ctrl
        ${CMAKE_CURRENT_LIST_DIR}/stm_lvgl_port
        ${CMAKE_CURRENT_LIST_DIR}/stm_ssd1306
        ${CMAKE_CURRENT_LIST_DIR}/UI
)

# Set component includes to parent scope so main CMakeLists.txt can use them
set(COMPONENT_INCLUDES ${COMPONENT_INCLUDES} PARENT_SCOPE)

