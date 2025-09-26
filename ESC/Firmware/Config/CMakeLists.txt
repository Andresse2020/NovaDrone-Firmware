cmake_minimum_required(VERSION 3.20)


file(GLOB D_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
)

# Create the static library
add_library(config_lib STATIC ${D_SRC_FILES})