file(GLOB_RECURSE MAIN_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/*.c
)
add_library(user_main INTERFACE)
target_sources(user_main INTERFACE ${MAIN_SOURCES})