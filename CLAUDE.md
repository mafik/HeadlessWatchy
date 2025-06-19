# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HeadlessWatchy is a ROM for the Watchy smartwatch that makes it usable without the e-paper display through vibration patterns. It's a dual-platform project:

1. **ESP32 Firmware** (C++/Arduino): Main watch firmware using PlatformIO
2. **Android Companion App** (Kotlin/Jetpack Compose): BLE-connected mobile app

The watch communicates time through vibration patterns and can trigger Tasker tasks on connected Android devices.

## Build Commands

### ESP32 Firmware
```bash
# Build and upload firmware to Watchy
make

# Or use PlatformIO directly
pio run --target upload --target monitor

# Build only (no upload)
pio run
```

### Android App
```bash
cd Android
./gradlew build

# Install debug APK
./gradlew installDebug
```

## Architecture

### ESP32 Firmware (`src/`)
- **main.cpp**: Core application logic, button handling, vibration patterns, WiFi time sync
- **BLE.cpp/h**: Bluetooth Low Energy communication with Android app
- **config.h**: Hardcoded WiFi credentials and timezone settings

### Key Features
- Vibration-based time indication (morse code patterns for hours/minutes)
- Button mappings for different functions (time, timer, BLE trigger)
- Imperial March melody played on startup via vibration motor
- WiFi time synchronization with NTP servers
- Timer functionality with countdown

### Android App (`Android/src/main/`)
- **MainActivity.kt**: Main app entry point
- **ble/**: BLE connection and device management
- **cdm/**: Companion Device Manager integration
- **WatchyCompanionDeviceService.kt**: Background service for BLE communication

### Hardware Pin Configuration
```cpp
#define VIB_MOTOR_PIN 13    // Vibration motor
#define MENU_BTN_PIN 26     // Upper left button
#define BACK_BTN_PIN 25     // Lower left button  
#define UP_BTN_PIN 32       // Upper right button
#define DOWN_BTN_PIN 4      // Lower right button
#define RTC_PIN GPIO_NUM_27 // Real-time clock
```

## Development Notes

- WiFi credentials are hardcoded in `config.h` - modify `kWiFiSSID` and `kWiFiPass` for your network
- Timezone is hardcoded as `kTZ` in `config.h` 
- BLE service UUID and characteristics are defined in `BLE.cpp`
- Debug output can be controlled via `kDebug` constant in `config.h`
- The project uses DS3232RTC library for real-time clock functionality