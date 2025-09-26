cmake_minimum_required(VERSION 3.20)


file(GLOB_RECURSE D_SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp

)

add_library(drivers_lib STATIC ${D_SRC_FILES})

target_link_libraries(drivers_lib PRIVATE
    board_lib                   # Board-specific library
    interface_actuators_lib     # Actuators interfaces
    interface_transport_lib     # Transport interfaces
    interface_sensors_lib       # Sensors interfaces
    interface_utilities_lib     # Utilities interfaces
    interface_system_lib        # System interfaces

)