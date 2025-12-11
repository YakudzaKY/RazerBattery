cmake_minimum_required(VERSION 3.10)
project(RazerBatteryTray)

set(CMAKE_CXX_STANDARD 17)

# Source files
set(SOURCES
    src/main.cpp
    src/RazerManager.cpp
    src/RazerDevice.cpp
    src/TrayIcon.cpp
)

# Headers
set(HEADERS
    src/RazerManager.h
    src/RazerDevice.h
    src/TrayIcon.h
)

add_executable(RazerBatteryTray WIN32 ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(RazerBatteryTray
    setupapi
    hid
    gdi32
    user32
    shell32
)
