# Xenochi

A phone-like smart device application built on ESP-IDF v5.5.1 with the ESP-Brookesia UI framework, targeting the Waveshare ESP32-C6-Touch-LCD-1.83 development board.

## Features

- **Phone-like OS** using ESP-Brookesia framework with home screen, app launcher, and status bar
- **Music Player** - Browse and play audio files from SD card
- **Settings App** - System configuration, battery info, WiFi scanning, backlight control
- **Gyroscope Monitor** - Real-time IMU sensor data display (QMI8658)
- **Audio Recorder** - Record audio to WAV files on SD card
- **MiBuddy** - Interactive animated character with sensor-driven behavior

## Target Hardware

| Specification | Value |
|---------------|-------|
| **Board** | [Waveshare ESP32-C6-Touch-LCD-1.83](https://www.waveshare.com/esp32-c6-touch-lcd-1.83.htm) |
| **MCU** | ESP32-C6 (RISC-V @ 160MHz), 512KB SRAM, 16MB Flash |
| **Display** | 1.83" IPS LCD, 240x284 pixels, ST7789 |
| **Touch** | CST816D capacitive touch (I2C) |
| **Power** | AXP2101 PMIC with Li-Po battery support |
| **Audio** | ES8311 codec, ES7210 ADC, NS4150B amplifier |
| **Sensors** | QMI8658C IMU, PCF85063A RTC |
| **Wireless** | Wi-Fi 6, Bluetooth 5 LE |

## Pin Configuration

| Peripheral | GPIO Pins |
|------------|-----------|
| **I2C** | SCL: GPIO8, SDA: GPIO7 |
| **LCD (SPI)** | SCK: GPIO1, MOSI: GPIO2, CS: GPIO5, DC: GPIO3, RST: GPIO4, BL: GPIO6 |
| **Touch** | INT: GPIO11 |
| **I2S Audio** | MCLK: GPIO19, SCLK: GPIO20, LRCK: GPIO22, SDIN: GPIO21, SDOUT: GPIO23 |
| **SD Card (SPI)** | CS: GPIO17, CLK: GPIO1, MISO: GPIO16, MOSI: GPIO2 |
| **Audio PA** | Enable: GPIO0 |

## Building

### Prerequisites

- [ESP-IDF v5.5.1](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32c6/get-started/index.html) or later
- Python 3.8+

### Build Commands

```bash
# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Configure for ESP32-C6
idf.py set-target esp32c6

# Build
idf.py build

# Flash and monitor (replace PORT with your serial port)
idf.py -p PORT flash monitor
```

## Project Structure

```
xenochi/
├── main/                               # Application entry point
│   ├── main.cpp                        # App initialization
│   └── 240_284/dark/                   # UI theme resources
│
├── components/
│   ├── bsp_esp32_c6_touch_lcd_1_83/   # Board Support Package
│   │   ├── include/bsp_board.h        # Pin definitions
│   │   └── peripherals/               # Peripheral drivers
│   │
│   ├── app_music/                      # Music Player
│   ├── app_setting/                    # Settings App
│   ├── app_gyroscope/                  # Gyroscope Monitor
│   ├── app_rec_test/                   # Audio Recorder
│   ├── app_mibuddy/                    # MiBuddy character
│   ├── audio_play/                     # Audio playback driver
│   └── XPowersLib/                     # AXP2101 PMU library
│
└── managed_components/                 # Third-party dependencies
```

## Adding a New App

1. Create a component in `components/app_yourapp/`
2. Implement a class inheriting from `ESP_Brookesia_PhoneApp`
3. Implement `run()`, `back()`, and `close()` methods
4. Add an app icon in `assets/`
5. Register in `main.cpp`:
   ```cpp
   YourAppConf *app = new YourAppConf(0, 0);
   phone->installApp(app);
   ```

## Dependencies

| Component | Version | Purpose |
|-----------|---------|---------|
| esp-brookesia | 0.5.0 | Phone UI framework |
| lvgl | 9.2.2 | Graphics library |
| esp_lvgl_port | 2.6.3 | LVGL ESP32 integration |
| esp_audio_simple_player | 0.9.5 | Audio playback |
| esp_lcd_touch_cst816s | 1.1.0 | Touch controller driver |
| qmi8658 | 1.0.1 | IMU driver |
| pcf85063a | 1.0.1 | RTC driver |

## Resources

- [Waveshare Wiki](https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.83)
- [Board Schematic](main/ESP32-C6-Touch-LCD-1.83_schematic.pdf)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/)
- [ESP-Brookesia Documentation](https://github.com/espressif/esp-brookesia)
- [LVGL Documentation](https://docs.lvgl.io/)

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## License

This project is provided as-is for educational and development purposes. Please refer to individual component licenses in `managed_components/` for third-party dependencies.
