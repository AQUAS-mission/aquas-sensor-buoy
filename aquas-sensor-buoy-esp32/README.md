# ESP32 Sensor Buoy - Aquas Sensor Buoy

This is the ESP32 version of the Aquas Sensor Buoy project. The code has been converted from Arduino to ESP32 with several key improvements for power efficiency and functionality.

## Key Changes from Arduino Version

### 1. **External SD Card Storage**

-   Uses HW125 SD card module for persistent data storage
-   Data is stored in `sensor.csv` on the SD card
-   Data survives deep sleep cycles (unlike SPIFFS)
-   Easy data retrieval by removing SD card

### 2. **ESP32 Deep Sleep**

-   Uses ESP32's built-in deep sleep functionality with timer wakeup
-   Uses `esp_sleep_enable_timer_wakeup()` for precise timing
-   Sleep duration: 1 hour (3,576,009,000 microseconds)
-   Much more power efficient than external RTC

### 3. **Peripheral Management**

-   WiFi disabled: `esp_wifi_stop()`
-   Bluetooth disabled: `esp_bt_controller_disable()`
-   ADC2 power released (used by WiFi)
-   All unnecessary peripherals turned off for maximum power efficiency

### 4. **ADC Improvements**

-   ESP32's 12-bit ADC (0-4095) instead of Arduino's 10-bit (0-1023)
-   Proper ADC calibration with `esp_adc_cal_characterize()`
-   Voltage range: 0-3.3V (adjusted from 5V Arduino)

### 5. **Time Management**

-   Uses DS3231 RTC module for accurate timekeeping
-   Automatic time synchronization on startup
-   Timestamps in ISO format: `YYYY-MM-DD HH:MM:SS`

## Hardware Requirements

### ESP32 Board

-   Any ESP32 development board (ESP32 DevKit, ESP32-WROOM, etc.)
-   4MB flash memory recommended
-   HW125 SD card module for data storage

### Sensors (Same as Arduino Version)

-   Atlas Scientific EZO sensors via I2C:
    -   pH sensor (I2C address 99)
    -   Dissolved Oxygen sensor (I2C address 97)
    -   Electrical Conductivity sensor (I2C address 100)
    -   RTD Temperature sensor (I2C address 102)
-   Turbidity sensor (analog input on GPIO34)

### Interlink Channels

-   Interlink isolated channel disable pin: GPIO26
-   Interlink non-isolated channel disable pin: GPIO27

## Pin Connections

| Component                      | ESP32 Pin | Notes                  |
| ------------------------------ | --------- | ---------------------- |
| I2C SDA                        | GPIO21    | Default ESP32 I2C SDA  |
| I2C SCL                        | GPIO22    | Default ESP32 I2C SCL  |
| Turbidity Sensor               | GPIO34    | ADC1_CH6, input only   |
| Interlink Isolated Disable     | GPIO25    | Output, HIGH = disable |
| Interlink Non-Isolated Disable | GPIO26    | Output, LOW = disable  |

### HW125 SD Card Module Connections

| SD Card Pin | ESP32 Pin | Notes                            |
| ----------- | --------- | -------------------------------- |
| VCC         | GPIO4     | Power control (HIGH=on, LOW=off) |
| GND         | GND       | Ground                           |
| MISO        | GPIO19    | SPI MISO (default)               |
| MOSI        | GPIO23    | SPI MOSI (default)               |
| SCK         | GPIO18    | SPI SCK (default)                |
| CS          | GPIO5     | Chip select (custom pin)         |

**Note**: Pin assignments have been optimized to avoid conflicts with SPI, I2C, and other critical functions. The VCC line is now controlled by GPIO4 for power management during deep sleep.

## Power Consumption

The ESP32 version is significantly more power efficient:

-   **Deep Sleep**: ~10μA (vs Arduino's ~50μA)
-   **Active Mode**: ~160mA during sensor readings
-   **SD Card Power**: Controlled via GPIO4 (powered off during sleep)
-   **Sleep Duration**: 1 hour between readings
-   **Estimated Battery Life**: 6-12 months with 3.7V LiPo battery

## Operation Cycle

1. **Wake Up**: ESP32 wakes from deep sleep every hour
2. **Initialize**: Disable peripherals, initialize SPIFFS, sync time
3. **Sensor Readings**: Wake sensors, take readings, sleep sensors
4. **Data Storage**: Write timestamp and sensor data to flash memory
5. **Deep Sleep**: Return to deep sleep for 1 hour

## Data Format

Data is stored in `sensor.csv` on the SD card with the following format:

```csv
timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity,turbidity_ntu
2024-01-15 14:30:00,7.25,23.5,8.2,1250.5,2.3
```

## Setup Instructions

### 1. Install Required Libraries

In Arduino IDE, install these libraries:

-   `Ezo_i2c_esp32` (Atlas Scientific ESP32 version)
-   `Ezo_i2c_util_esp32` (Atlas Scientific ESP32 version)
-   `DS3231` (RTC library)
-   `SD` (built-in ESP32 library)

### 2. Configure Board Settings

-   Board: "ESP32 Dev Module" (or your specific ESP32 board)
-   Upload Speed: 115200
-   CPU Frequency: 240MHz
-   Flash Frequency: 80MHz
-   Flash Mode: QIO
-   Flash Size: 4MB (32Mb)
-   Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)

### 3. Upload Code

-   Upload the `aquas-sensor-buoy-esp32.ino` file
-   The ESP32 will automatically create the CSV file on the SD card on first run

### 4. Monitor Output

Connect to Serial Monitor (115200 baud) to see:

-   System initialization
-   Sensor readings
-   Data storage confirmation
-   Sleep cycle information

## Data Retrieval

### Method 1: Serial Monitor

Data is printed to Serial during operation for debugging.

### Method 2: SD Card (Recommended)

Simply remove the SD card and read it on your computer:

-   Insert SD card into computer or card reader
-   Open `sensor.csv` with any spreadsheet application
-   Data is preserved across deep sleep cycles

### Method 3: WiFi Upload (Optional)

You can add WiFi functionality temporarily to upload data:

```cpp
// Add to setup() for data upload
WiFi.begin("SSID", "PASSWORD");
// Upload data via HTTP or FTP
```

## Troubleshooting

### Common Issues

1. **SD Card Mount Failed**

    - Ensure SD card is properly formatted (FAT32)
    - Check wiring connections (VCC, GND, MISO, MOSI, SCK, CS)
    - Verify SD card is not corrupted

2. **Sensor Communication Issues**

    - Check I2C connections (SDA/SCL)
    - Verify sensor addresses
    - Ensure proper power supply to sensors

3. **Deep Sleep Not Working**

    - Check that no GPIO pins are floating
    - Ensure proper power supply during sleep

4. **Time Sync Failed**
    - Check internet connection (if using WiFi)
    - Adjust timezone in `setupTime()` function

## Customization

### Change Sleep Duration

```cpp
const uint64_t SLEEP_DURATION = 1800000000ULL; // 30 minutes
```

### Add WiFi for Data Upload

```cpp
// In setup(), after sensor readings
WiFi.begin("SSID", "PASSWORD");
// Upload data to server
```

### Change SD Card CS Pin

```cpp
const int sdCardCSPin = 4; // Change from GPIO5 to GPIO4 if needed
```

### Modify Sensor Addresses

```cpp
Ezo_board PH = Ezo_board(99, "PH"); // Change 99 to your sensor's address
```

## Power Optimization Tips

1. **Use 3.3V Logic Level**: All sensors should operate at 3.3V
2. **Minimize Active Time**: Keep sensor readings brief
3. **Disable Debug Output**: Remove Serial.print() statements for production
4. **SD Card Power**: SD cards can draw significant current - consider powering down during sleep
5. **Battery Monitoring**: Add voltage divider for battery level monitoring

## License

This project is based on the original Arduino sensor buoy code, adapted for ESP32 with power optimization improvements.
