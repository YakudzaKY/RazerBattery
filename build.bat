@echo off
setlocal enabledelayedexpansion

echo [INFO] Checking environment...

REM Check if cl.exe is already available
where cl.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo [INFO] C++ compiler - cl.exe - found in PATH.
    goto :build
)

echo [INFO] Searching for Visual Studio...

set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" (
    echo [ERROR] 'vswhere.exe' not found. Please install Visual Studio with C++ workload.
    pause
    exit /b 1
)

REM Find latest VS with C++ tools
for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "vs_path=%%i"
)

if "!vs_path!"=="" (
    echo [ERROR] No suitable Visual Studio installation found.
    echo [HINT] Install the 'Desktop development with C++' workload.
    pause
    exit /b 1
)

echo [INFO] Found Visual Studio at: !vs_path!
set "vcvars=!vs_path!\VC\Auxiliary\Build\vcvars64.bat"

if not exist "!vcvars!" (
    echo [ERROR] Environment script not found: !vcvars!
    pause
    exit /b 1
)

echo [INFO] Initializing environment...
call "!vcvars!" >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Failed to initialize environment.
    pause
    exit /b 1
)

:build
if not exist build mkdir build
cd build

REM Clean cache to avoid generator mismatches
if exist CMakeCache.txt del CMakeCache.txt

echo [INFO] Configuring CMake...
cmake .. -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 (
    echo [ERROR] Configuration failed.
    pause
    exit /b 1
)

echo [INFO] Building Release...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    pause
    exit /b 1
)

echo.
echo [SUCCESS] Build finished successfully.
echo [OUTPUT] build\Release\RazerBatteryTray.exe
pause
