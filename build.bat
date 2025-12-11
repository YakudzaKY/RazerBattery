@echo off
setlocal
echo Building Razer Battery Tray...

REM Check if cl.exe is in the path (Developer Command Prompt)
where cl.exe >nul 2>nul
if %errorlevel% equ 0 (
    echo Developer environment detected.
    goto :build
)

echo Developer environment not detected. Searching for Visual Studio...

set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" (
    echo Error: Visual Studio Installer not found. Please install Visual Studio or run this script from a Developer Command Prompt.
    pause
    exit /b 1
)

REM Find the latest Visual Studio with C++ tools
for /f "usebackq tokens=*" %%i in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "vs_path=%%i"
)

if not defined vs_path (
    echo Error: No suitable Visual Studio installation found.
    pause
    exit /b 1
)

echo Found Visual Studio at: %vs_path%

REM Try to set up the x64 environment
if exist "%vs_path%\VC\Auxiliary\Build\vcvars64.bat" (
    call "%vs_path%\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo Error: vcvars64.bat not found.
    pause
    exit /b 1
)

if %errorlevel% neq 0 (
    echo Error: Failed to set up build environment.
    pause
    exit /b 1
)

:build
if not exist build (
    mkdir build
)

cd build

REM Clean CMakeCache.txt to ensure a fresh configuration
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
endlocal
