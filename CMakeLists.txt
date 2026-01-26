cmake_minimum_required(VERSION 3.22)

if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_BUILD_TYPE "Debug")

add_compile_options(-Wall -Wextra)

project(G474_1 C ASM)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_executable(${PROJECT_NAME})

# --- STM32CubeMX ---
add_subdirectory(cmake/stm32cubemx)

# --- LVGL Configuration ---
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Disable demos" FORCE)
set(LV_BUILD_CONF_PATH "${CMAKE_SOURCE_DIR}/User/components/lvgl_port/lv_conf.h" CACHE PATH "Path to lv_conf.h" FORCE)
add_subdirectory(lvgl)
target_link_libraries(lvgl PRIVATE stm32cubemx) # LVGL 需要依赖 STM32 的库 (FreeRTOS, HAL等)

# --- User Modules ---
add_subdirectory(User/components)
add_subdirectory(User/main)

target_link_libraries(${PROJECT_NAME}
        stm32cubemx
        lvgl
        user_components
        user_main
)

target_link_options(${PROJECT_NAME} PRIVATE -u _printf_float)

list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)