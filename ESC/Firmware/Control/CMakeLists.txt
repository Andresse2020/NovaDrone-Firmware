cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE C_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(control_lib ${C_SRC_FILES})

target_include_directories(control_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/API)

target_link_libraries(control_lib PRIVATE 
    services_lib                # Services library
    interface_protocol_lib      # Protocol interfaces
)