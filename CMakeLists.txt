cmake_minimum_required(VERSION 3.10)
project(RazerBatteryTray LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ensure Unicode
add_definitions(-DUNICODE -D_UNICODE)

include_directories(include)
include_directories(libusb/include)

# Find all source files
file(GLOB SOURCES "src/*.cpp")

# Create Windows Application (WIN32 means no console window by default)
add_executable(RazerBatteryTray WIN32 ${SOURCES})

# Keep a single stable output path to avoid launching stale binaries from build root.
set_target_properties(RazerBatteryTray PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)

# Link Windows libraries
target_link_libraries(RazerBatteryTray
    setupapi
    hid
    gdi32
    user32
    kernel32
    shell32
    advapi32
    "${CMAKE_SOURCE_DIR}/libusb/VS2022/MS64/static/libusb-1.0.lib"
)
