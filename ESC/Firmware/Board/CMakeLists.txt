cmake_minimum_required(VERSION 3.20)

add_library(board_lib STATIC)


file(GLOB_RECURSE BOARD_SRC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp

)

file(GLOB_RECURSE BOARD_INC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp
)

# Specify the device series (adjust according to your STM32 series)
set(Device STM32G4xx)

target_include_directories(board_lib PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/BSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/Peripherals
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/Common

	${CMAKE_SOURCE_DIR}/ThirdParty/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/ThirdParty/CMSIS/Device/ST/${Device}/Include
    ${CMAKE_SOURCE_DIR}/ThirdParty/${Device}_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/ThirdParty/${Device}_HAL_Driver/Inc/Legacy
)

# Add the HAL source files
file(GLOB HAL_SRC ${CMAKE_SOURCE_DIR}/ThirdParty/${Device}_HAL_Driver/Src/*.c)

# Exclude template files if any
list(FILTER HAL_SRC EXCLUDE REGEX ".*_template.c$")

# Define the startup file
file(GLOB STARTUP_FILE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/Startup/*.s)

# 3) (Optional) If your file uses .s (without preprocessing), uncomment below:
set_source_files_properties(${STARTUP_FILE_SRC} PROPERTIES LANGUAGE ASM)

# Add all source files to the library target
target_sources(board_lib PUBLIC
    ${BOARD_SRC} 
    ${HAL_SRC} 
    ${STARTUP_FILE_SRC}
)

