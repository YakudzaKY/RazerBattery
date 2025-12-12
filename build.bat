@echo off
setlocal

echo [INFO] checking for CMake...
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake not found. Please install CMake and add it to PATH.
    pause
    exit /b 1
)

if not exist build mkdir build
cd build

echo [INFO] Generating build files...
cmake .. -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] CMake generation failed. Ensure Visual Studio (C++) is installed.
    pause
    exit /b 1
)

echo [INFO] Building project...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo [INFO] Build successful! Output is in build\Release\
pause
