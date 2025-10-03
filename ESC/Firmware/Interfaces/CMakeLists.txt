cmake_minimum_required(VERSION 3.20)  # Require at least CMake 3.20

# --------------------------------------------------------------------------
# Define interface libraries and their corresponding source directories
# Each entry follows the format: library_name:directory_name
# Example: actuators:Actuators
# --------------------------------------------------------------------------
set(INTERFACE_LIBS
    actuators:Actuators
    transport:Transport
    sensors:Sensors
    utilities:Utilities
    system:ISystem
)

# --------------------------------------------------------------------------
# Iterate over each entry to create INTERFACE libraries and configure include paths
# --------------------------------------------------------------------------
foreach(lib_info ${INTERFACE_LIBS})

    # ----------------------------------------------------------------------
    # Split the string at ":" to separate the logical library name and directory
    # This converts "actuators:Actuators" into a list: ["actuators", "Actuators"]
    # ----------------------------------------------------------------------
    string(REPLACE ":" ";" lib_parts ${lib_info})

    # ----------------------------------------------------------------------
    # Extract the library name (first element) and the corresponding folder (second element)
    # ----------------------------------------------------------------------
    list(GET lib_parts 0 lib_name)  # e.g., "actuators"
    list(GET lib_parts 1 lib_dir)   # e.g., "Actuators"

    # ----------------------------------------------------------------------
    # Create an INTERFACE library named interface_<lib_name>_lib
    # INTERFACE libraries are header-only:
    #   - They do not generate object code
    #   - They propagate include directories, compile options, and dependencies to consumers
    # Example: interface_actuators_lib
    # ----------------------------------------------------------------------
    add_library(interface_${lib_name}_lib INTERFACE)

    # ----------------------------------------------------------------------
    # Add the corresponding include directory to the interface library
    # The INTERFACE keyword ensures that any target linking this library
    # automatically sees the headers without needing additional configuration
    # ----------------------------------------------------------------------
    target_include_directories(interface_${lib_name}_lib INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR}/${lib_dir}
    )

endforeach()
