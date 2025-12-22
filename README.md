# Xenochi - ESP32-C6 Smart Device Application

A phone-like smart device application built on ESP-IDF v5.5.1 with the ESP-Brookesia UI framework, targeting the ESP32-C6 with a 1.83" touchscreen LCD (240x284 pixels).

## Project Overview

This project implements a multi-app smart device with a modern phone-style interface. It features:

- **Phone-like OS** using ESP-Brookesia framework with home screen, app launcher, status bar
- **Music Player** - Browse and play audio files from SD card
- **Settings App** - System configuration, battery info, WiFi scanning, backlight control
- **Gyroscope Monitor** - Real-time IMU sensor data display (QMI8658)
- **Audio Recorder** - Record audio to WAV files on SD card

## Hardware Configuration

### Target Board
- **MCU**: ESP32-C6
- **Display**: 1.83" LCD, 240x284 pixels, SPI interface
- **Touch**: CST816S capacitive touch controller (I2C)
- **Power Management**: AXP2101 PMU
- **IMU**: QMI8658 6-axis (accelerometer + gyroscope)
- **RTC**: PCF85063A real-time clock
- **Audio**: I2S interface with stereo support (16kHz, 16-bit)
- **Storage**: SD card via SPI

### Pin Configuration

| Peripheral | GPIO Pins |
|------------|-----------|
| **I2C** | SCL: GPIO8, SDA: GPIO7 |
| **LCD (SPI)** | SCK: GPIO1, MOSI: GPIO2, CS: GPIO5, DC: GPIO3, RST: GPIO4, BL: GPIO6 |
| **Touch** | INT: GPIO11 |
| **I2S Audio** | MCLK: GPIO19, SCLK: GPIO20, LRCK: GPIO22, SDIN: GPIO21, SDOUT: GPIO23 |
| **SD Card (SPI)** | CS: GPIO17, CLK: GPIO1, MISO: GPIO16, MOSI: GPIO2 |
| **Audio PA** | Enable: GPIO0 |

## Project Structure

```
xenochi/
├── main/                               # Application entry point
│   ├── main.cpp                        # App initialization and phone setup
│   ├── CMakeLists.txt                  # Main component build config
│   ├── Kconfig.projbuild               # Build configuration options
│   └── 240_284/dark/                   # UI theme resources
│       ├── stylesheet.hpp              # Phone UI stylesheet
│       └── *.hpp                       # UI component data files
│
├── components/                         # Custom ESP-IDF components
│   │
│   ├── bsp_esp32_c6_touch_lcd_1_83/   # Board Support Package (BSP)
│   │   ├── include/bsp_board.h        # BSP public API header
│   │   ├── boards/bsp_board.c         # Board initialization
│   │   ├── peripherals/
│   │   │   ├── bsp_lcd.c              # LCD driver (SPI, ST7789-like)
│   │   │   ├── bsp_touch.c            # Touch driver (CST816S)
│   │   │   ├── bsp_i2c_master.c       # I2C bus driver
│   │   │   ├── bsp_i2s.c              # I2S audio interface
│   │   │   ├── bsp_lvgl_display.c     # LVGL display integration
│   │   │   ├── bsp_axp2101.cpp        # Power management IC driver
│   │   │   ├── bsp_rtc.c              # RTC driver (PCF85063A)
│   │   │   ├── bsp_imu.c              # IMU driver (QMI8658)
│   │   │   ├── bsp_sdcard.c           # SD card driver
│   │   │   ├── bsp_wifi_port.c        # WiFi interface
│   │   │   └── bsp_codec.c            # Audio codec driver
│   │   ├── CMakeLists.txt
│   │   └── idf_component.yml
│   │
│   ├── app_music/                      # Music Player Application
│   │   ├── lvgl_app_music.cpp         # App class implementation
│   │   ├── lvgl_music.c               # Music functionality
│   │   ├── lvgl_music_main.c          # Main music player UI
│   │   ├── lvgl_music_list.c          # Music list UI
│   │   ├── include/                   # Header files
│   │   └── assets/                    # App icon and images
│   │
│   ├── app_setting/                    # Settings Application
│   │   ├── lvgl_app_setting.cpp       # Settings app (battery, WiFi, etc.)
│   │   ├── wifi_scan.c                # WiFi scanning functionality
│   │   ├── include/
│   │   └── assets/
│   │
│   ├── app_gyroscope/                  # Gyroscope Monitor Application
│   │   ├── lvgl_app_gyroscope.cpp     # Real-time IMU data display
│   │   ├── include/
│   │   └── assets/
│   │
│   ├── app_rec_test/                   # Audio Recorder Application
│   │   ├── lvgl_app_rec.cpp           # WAV recording to SD card
│   │   ├── include/
│   │   └── assets/
│   │
│   ├── audio_play/                     # Audio Playback Driver
│   │   ├── audio_driver.c             # Audio playback engine
│   │   ├── include/audio_driver.h     # Audio API
│   │   ├── CMakeLists.txt
│   │   └── idf_component.yml
│   │
│   └── XPowersLib/                     # AXP2101 PMU Library
│       ├── src/                       # Library source
│       └── src/REG/                   # PMU register definitions
│
├── managed_components/                 # Third-party dependencies
│   ├── espressif__esp-brookesia/      # Phone UI framework (v0.5.0)
│   ├── lvgl__lvgl/                    # LVGL graphics library (v9.2.2)
│   ├── espressif__esp_lvgl_port/      # LVGL ESP32 port (v2.6.3)
│   ├── espressif__esp_audio_simple_player/  # Audio player (v0.9.5)
│   ├── espressif__esp_lcd_touch_cst816s/    # Touch driver (v1.1.0)
│   ├── waveshare__qmi8658/            # IMU driver (v1.0.1)
│   ├── waveshare__pcf85063a/          # RTC driver (v1.0.1)
│   └── ...                            # Other audio/GMF components
│
├── CMakeLists.txt                      # Root build configuration
├── sdkconfig                           # ESP-IDF configuration
├── sdkconfig.defaults                  # Default configuration
├── partitions.csv                      # Flash partition layout
└── dependencies.lock                   # Locked dependency versions
```

## Application Architecture

### Startup Flow (`main.cpp`)

1. **Board Initialization** (`bsp_init()`)
   - I2C, SPI, I2S peripherals
   - LCD and touch drivers
   - SD card mounting
   - RTC and IMU initialization

2. **Power Management** (`AXP2101_driver_init()`)
   - Configure power rails
   - Battery monitoring setup

3. **Music File Discovery** (`LVGL_Search_Music()`)
   - Scan SD card for audio files

4. **Phone UI Setup**
   - Create ESP-Brookesia Phone object
   - Apply 240x284 dark theme stylesheet
   - Configure touch input

5. **App Installation**
   - Music Player (`PhoneMusicConf`)
   - Settings (`PhoneSettingConf`)
   - Gyroscope (`PhoneGyroscopeConf`)
   - Recorder (`PhoneRecConf`)

6. **Clock Timer**
   - 1-second timer to update status bar clock

### App Lifecycle

Each app inherits from `ESP_Brookesia_PhoneApp` and implements:

| Method | Purpose |
|--------|---------|
| `run()` | Called when app launches, creates UI |
| `back()` | Handle back button, typically calls `notifyCoreClosed()` |
| `close()` | Cleanup when app exits |

### Audio Subsystem

The audio driver (`audio_driver.c`) uses a command queue pattern:

- **Commands**: `PLAY`, `STOP`, `PAUSE`, `RESUME`, `DEINIT`
- **Task**: Dedicated FreeRTOS task processes commands
- **Pipeline**: ESP Audio Simple Player for file decoding
- **Output**: I2S via BSP codec driver

## Building the Project

### Prerequisites

- ESP-IDF v5.5.1 or later
- Python 3.8+

### Build Commands

```bash
# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Configure for ESP32-C6
idf.py set-target esp32c6

# Build
idf.py build

# Flash and monitor
idf.py -p PORT flash monitor
```

## Memory Layout

From `partitions.csv`:

| Partition | Offset | Size |
|-----------|--------|------|
| NVS | 0x9000 | 24 KB |
| Factory | 0x10000 | 4 MB |

## Key Configuration Options

From `sdkconfig`:

- **Display**: 240x284 pixels, 16-bit color
- **Touch**: CST816S on I2C
- **Audio**: 16kHz, 16-bit stereo
- **Default Backlight**: 90%
- **Default Volume**: 95%

## Modifying the Code

### Adding a New App

1. Create component in `components/app_yourapp/`
2. Implement class inheriting `ESP_Brookesia_PhoneApp`
3. Implement `run()`, `back()`, `close()` methods
4. Add app icon in `assets/`
5. Register in `main.cpp`:
   ```cpp
   YourAppConf *app = new YourAppConf(0, 0);
   phone->installApp(app);
   ```

### Modifying Hardware Pins

Edit `components/bsp_esp32_c6_touch_lcd_1_83/include/bsp_board.h`:
- GPIO definitions are at the top of the file
- Update the corresponding driver if needed

### Adjusting UI Theme

Modify `main/240_284/dark/stylesheet.hpp` for:
- Colors, fonts, sizes
- Status bar configuration
- Navigation bar style

## Dependencies

Key managed components:

| Component | Version | Purpose |
|-----------|---------|---------|
| esp-brookesia | 0.5.0 | Phone UI framework |
| lvgl | 9.2.2 | Graphics library |
| esp_lvgl_port | 2.6.3 | LVGL ESP32 integration |
| esp_audio_simple_player | 0.9.5 | Audio playback |
| esp_lcd_touch_cst816s | 1.1.0 | Touch controller driver |
| qmi8658 | 1.0.1 | IMU driver |
| pcf85063a | 1.0.1 | RTC driver |

## License

Please refer to individual component licenses in `managed_components/`.
