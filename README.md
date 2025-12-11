# Razer Battery Tray

An optimized Windows application that monitors connected Razer devices and displays their battery status and type in the system tray.

## Features

- **Multi-Device Support:** Displays a separate tray icon for each connected Razer device.
- **Dynamic Icons:** Icons update dynamically to show device type (e.g., 'M' for Mouse, 'K' for Keyboard, 'H' for Headset) and battery percentage.
- **Charging Status:** Indicates when a device is charging.
- **Optimized Performance:**
  - Uses Windows Event API (`RegisterDeviceNotification`) to detect device connections/disconnections instantly without polling.
  - Uses a low-frequency timer (every 5 minutes) to query battery levels, minimizing system overhead.
- **Zero-Config:** Automatically detects compatible devices.

## Build Instructions (Инструкция по сборке)

This project uses CMake. You need **CMake** and a C++ compiler (like **Visual Studio** or **MinGW**) installed on Windows.

### Quick Build (Windows)

Simply double-click `build.bat`.

### Manual Build

1.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
2.  Generate build files (for Visual Studio):
    ```bash
    cmake ..
    ```
3.  Build the project:
    ```bash
    cmake --build . --config Release
    ```

### Как скомпилировать (Russian)

Для сборки вам понадобится **CMake** и компилятор C++ (например, **Visual Studio 2019/2022**).

1.  **Простой способ:** Запустите файл `build.bat`. Он автоматически создаст папку build и скомпилирует проект.
2.  **Ручной способ:**
    - Откройте командную строку (cmd или PowerShell).
    - Перейдите в папку проекта.
    - Выполните команды:
      ```cmd
      mkdir build
      cd build
      cmake ..
      cmake --build . --config Release
      ```
    - Исполняемый файл `RazerBatteryTray.exe` появится в папке `build\Release`.

## Credits & Acknowledgements

- **OpenRazer:** The `driver/` directory in this repository contains source code from the [OpenRazer](https://github.com/openrazer/openrazer) project. It is included here solely as a reference for reverse-engineering the Razer HID protocol. This application is a clean-room implementation of the Windows-side logic based on those protocol details.

## License

This project is licensed under the MIT License - see the LICENSE file for details.
The `driver/` content belongs to the OpenRazer project and is licensed under the GPL-2.0.
