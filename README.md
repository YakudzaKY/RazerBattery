# Razer Battery Tray

Native C++ Windows tray application that reads battery state from supported gaming peripherals and shows one tray icon per device.

Current transports:

- **Razer:** USB HID over `libusb`
- **ASUS ROG Spatha X:** direct Windows HID (`hid` / `setupapi`), no Armoury Crate required

## Current Behavior

- **Multi-device UI:** Separate tray icon for each detected supported device.
- **Device names in tooltip:** Tooltips use the actual device name when available, for example `ROG SPATHA X: 93% (Charging)`.
- **Placeholder mode:** If no supported devices are detected, a `No` tray icon is shown with tooltip `No Supported Devices Found`.
- **Battery rendering:** The icon shows a large battery number (`0-100`) or `-` when a device is offline/unreachable.
- **Charging/offline state:** Tooltip includes charging/offline state.
- **ASUS autonomy:** `ROG Spatha X` battery and charging status are read directly from the device. ASUS background software is not used at runtime.
- **Type-aware background colors (tray readability):**
  - Mouse: dark green
  - Headset: dark blue
  - Keyboard: dark amber
  - Accessory: dark violet
- **Level/state text colors:**
  - Green: normal
  - Yellow: below 50%
  - Red: below 20%
  - Cyan: charging
  - Gray: offline

## Performance Strategy

- Battery refresh timer is **1 minute** (`UPDATE_INTERVAL_MS = 60000`).
- Device hotplug is handled with `RegisterDeviceNotification` + `WM_DEVICECHANGE`.
- Re-enumeration after device-change events is delayed/debounced by **1200 ms**.
- No high-frequency polling loop.

## Protocol Notes (Implementation)

### Razer

- Main battery query: command class/id `0x07 / 0x80`.
- Headset-specific fallback query: `0x0F / 0x02` (used for some models).
- Charging status query: `0x07 / 0x84`.
- Transaction IDs are tried by PID preference across `0x1F`, `0x3F`, `0xFF`.
- Transport fallback is implemented:
  - Feature report path first.
  - Output+Input report path second.
- `0%` is currently treated as offline (`-`) to avoid false zero reports after link-state changes.

### ASUS

- Current direct support is implemented for **ROG Spatha X** (`0x18F4`, `0x1977`, `0x1979`).
- The app opens the vendor HID interface (`MI_00`) and sends a direct battery query.
- Query/parse logic is isolated in `AsusProtocol`.
- No ASUS DLLs, services, logs, or Armoury Crate files are required at runtime.

## Build Requirements

- Windows x64
- CMake 3.10+
- Visual Studio with C++ workload (MSVC)

Note: current `CMakeLists.txt` links directly to:
`libusb/VS2022/MS64/static/libusb-1.0.lib`
So MinGW is not configured out of the box.

## Build

### Quick Build (Windows)

Run `build.bat`.

### Manual Build

```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
ctest -C Release --output-on-failure
```

Output binary: `build\RazerBatteryTray.exe`

## Runtime Notes

- The app runs in the background via a hidden message window and tray icons.
- Single-instance protection is enabled (`Global\RazerBatteryTray_Instance_Mutex`).
- Exit flow: right-click a tray icon and select `Exit`.
- Optional debug log: set `RAZERBATTERY_LOG=1` before launching.

## Tests

- `AsusProtocolTest` covers the direct ASUS battery query packet and response parsing, including charging-state decoding.

## Device ID Source

- `include/DeviceIds.h` auto-generated from OpenRazer headers using `generate_ids.py`.
- `driver/` is reference material only. Do not compile or link it into this app.

## Credits

- OpenRazer protocol reference: [https://github.com/openrazer/openrazer](https://github.com/openrazer/openrazer)

## License

- `driver/` content belongs to the OpenRazer project (GPL-2.0).
- This repository currently has no top-level `LICENSE` file for the app's own source files.
