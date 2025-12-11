@echo off
echo Building Razer Battery Tray...

if not exist build (
    mkdir build
)

cd build

REM Clean CMakeCache.txt to ensure a fresh configuration and avoid "platform x64" errors
REM from previous failed runs.
if exist CMakeCache.txt (
    del CMakeCache.txt
)

cmake .. -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo CMake generation failed!
    pause
    exit /b %errorlevel%
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b %errorlevel%
)

echo Build successful! Executable is in build\Release
pause
