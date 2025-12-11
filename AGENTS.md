# AI Agent Context

This file provides context for AI agents working on the Razer Battery Tray project.

## Project Overview

This is a native C++ Windows application designed to be lightweight and optimized. Its primary function is to query battery levels from Razer peripherals via USB HID feature reports and display this information in the Windows System Tray.

## Repository Structure

- **`src/`**: Contains the source code for this application.
  - `main.cpp`: Entry point, window message loop, and timer setup.
  - `RazerManager`: Handles device enumeration (SetupAPI), device change notifications (`WM_DEVICECHANGE`), and the list of active devices.
  - `RazerDevice`: Encapsulates the HID communication logic. It sends feature reports to get battery and charging status.
  - `TrayIcon`: Manages the Windows Notify Icon (Tray Icon), including dynamic icon generation using GDI.
- **`driver/`**: **Reference Material Only.** This directory contains source code from the [OpenRazer](https://github.com/openrazer/openrazer) Linux driver project.
  - **Do not compile or modify files in `driver/`.**
  - **Do not link against code in `driver/`.**
  - **Use `driver/` to understand the Razer USB Protocol.** Specifically, look at `razermouse_driver.c` and `razercommon.h` to understand Command Classes, Command IDs, and Report Structures.

## Directives for AI Agents

1.  **Optimization is Key:** The user explicitly requested an optimized application.
    - Avoid high-frequency polling. We use `RegisterDeviceNotification` to detect hardware changes efficiently.
    - Battery queries are done on a slow timer (e.g., 5 minutes) because battery levels do not change rapidly.
    - Keep resource usage (CPU/RAM) low.

2.  **Protocol Implementation:**
    - When adding support for new devices or features, consult the `driver/` directory.
    - Note that Windows HID requires specific handling of Report IDs. Razer usually uses Report ID 0.
    - Check `razermouse_driver.c` for Transaction ID logic (some devices use 0x1F, some 0x3F, some 0xFF).

3.  **Windows API:**
    - The application uses a hidden top-level window to receive `WM_DEVICECHANGE` and `WM_TIMER` messages.
    - UI is purely Tray Icons.

4.  **OpenRazer Context:**
    - Always respect the origin of the `driver/` folder. It is external code provided for documentation purposes regarding the wire protocol.
