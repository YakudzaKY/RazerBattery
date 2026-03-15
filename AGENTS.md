# AI Agent Context

This file provides context for AI agents working on the Razer Battery Tray project.

## Project Overview

This is a native C++ Windows application designed to be lightweight and optimized. It queries battery state from supported peripherals and displays one icon per device in the Windows System Tray.

Current device paths:

- **Razer:** USB HID protocol over `libusb`
- **ASUS ROG Spatha X:** direct Windows HID on the vendor interface (`hid` / `setupapi`), no Armoury Crate dependency

## Repository Structure

- **`src/`**: Contains the source code for this application.
  - `main.cpp`: Entry point, window message loop, and timer setup.
  - `DeviceManager`: Handles USB enumeration, device change notifications (`WM_DEVICECHANGE`), and the list of active devices.
  - `RazerDevice`: Encapsulates the Razer HID communication logic over `libusb`.
  - `AsusDevice`: Encapsulates direct ASUS HID communication and device discovery for supported ASUS devices.
  - `AsusProtocol`: Isolated query/parse helpers for direct ASUS battery packets.
  - `TrayIcon`: Manages the Windows Notify Icon (Tray Icon), including dynamic icon generation using GDI.
- **`tests/`**:
  - `AsusProtocolTest.cpp`: Lightweight protocol-level tests for ASUS query/response handling.
- **`driver/`**: **Reference Material Only.** This directory contains source code from the [OpenRazer](https://github.com/openrazer/openrazer) Linux driver project.
  - **Do not compile or modify files in `driver/`.**
  - **Do not link against code in `driver/`.**
  - **Use `driver/` to understand the Razer USB Protocol only.** Specifically, look at `razermouse_driver.c` and `razercommon.h` to understand Command Classes, Command IDs, and Report Structures.

## Directives for AI Agents

1.  **Optimization is Key:** The user explicitly requested an optimized application.
    - Avoid high-frequency polling. We use `RegisterDeviceNotification` to detect hardware changes efficiently.
    - Battery queries are done on a slow timer. The current global UI refresh interval is **1 minute**.
    - `AsusDevice` has an additional short internal debounce to avoid duplicate direct HID reads.
    - Keep resource usage (CPU/RAM) low.

2.  **Protocol Implementation:**
    - When adding or debugging **Razer** support, consult the `driver/` directory.
    - Note that Windows HID requires specific handling of Report IDs. Razer usually uses Report ID 0.
    - Check `razermouse_driver.c` for Transaction ID logic (some devices use 0x1F, some 0x3F, some 0xFF).
    - When adding **ASUS** support, prefer direct HID implementations. Do not reintroduce runtime dependence on Armoury Crate logs, DLLs, or services unless explicitly requested.
    - Keep protocol parsing isolated when practical. `AsusProtocol` is the current example of that split.

3.  **Windows API:**
    - The application uses a hidden top-level window to receive `WM_DEVICECHANGE` and `WM_TIMER` messages.
    - UI is purely Tray Icons.
    - The tray tooltip should show the real device name when available, not just the generic device type.

4.  **OpenRazer Context:**
    - Always respect the origin of the `driver/` folder. It is external code provided for documentation purposes regarding the Razer wire protocol.
