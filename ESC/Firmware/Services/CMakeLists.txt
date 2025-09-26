cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE S_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(services_lib ${S_SRC_FILES})

target_include_directories(services_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/API)

target_link_libraries(services_lib PRIVATE 
    interface_actuators_lib     # Actuators interfaces
    interface_transport_lib     # Transport interfaces
    interface_sensors_lib       # Sensors interfaces
    interface_utilities_lib     # Utilities interfaces
    interface_system_lib        # System interfaces
    interface_protocol_lib      # Protocol interfaces
)