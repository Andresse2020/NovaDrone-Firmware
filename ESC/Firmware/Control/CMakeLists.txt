cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE C_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(control_API ${C_SRC_FILES})

target_include_directories(control_API PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/API)

target_link_libraries(control_API PRIVATE 
    services_API                # Services library
    # interface_protocol_lib      # Protocol interfaces
)