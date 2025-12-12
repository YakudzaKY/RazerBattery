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
