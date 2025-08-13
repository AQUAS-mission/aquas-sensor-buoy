# Arduino Sensor Buoy - Aquas Sensor Buoy

This is the original Arduino version of the Aquas Sensor Buoy project. This version uses external hardware components for data storage and timing.

## Hardware Requirements

### Arduino Board

-   Arduino Mega 2560 (recommended for multiple I2C sensors)
-   Alternative: Arduino Uno or Nano (with limitations)

### External Components

-   **SD Card Module**: For data storage
    -   Chip Select: Pin 53 (Mega) or Pin 10 (Uno/Nano)
    -   SPI connections (MOSI, MISO, SCK)
-   **DS3231 RTC Module**: For precise timing and wake-up
    -   I2C connections (SDA, SCL)
    -   Interrupt pin: Pin 2
-   **Power Management**: External power supply for long-term operation

### Sensors

-   Atlas Scientific EZO sensors via I2C:
    -   pH sensor (I2C address 99)
    -   Dissolved Oxygen sensor (I2C address 97)
    -   Electrical Conductivity sensor (I2C address 100)
    -   RTD Temperature sensor (I2C address 102)
-   Turbidity sensor (analog input on A1)

### Interlink Channels

-   Interlink isolated channel disable pin: Pin 3
-   Interlink non-isolated channel disable pin: Pin 4

## Pin Connections

| Component                      | Arduino Pin          | Notes                   |
| ------------------------------ | -------------------- | ----------------------- |
| I2C SDA                        | A4                   | Default Arduino I2C SDA |
| I2C SCL                        | A5                   | Default Arduino I2C SCL |
| Turbidity Sensor               | A1                   | Analog input            |
| RTC Interrupt                  | 2                    | Wake-up interrupt       |
| Interlink Isolated Disable     | 3                    | Output, HIGH = disable  |
| Interlink Non-Isolated Disable | 4                    | Output, LOW = disable   |
| SD Card CS                     | 53 (Mega) / 10 (Uno) | Chip Select             |
| SD Card MOSI                   | 51 (Mega) / 11 (Uno) | SPI MOSI                |
| SD Card MISO                   | 50 (Mega) / 12 (Uno) | SPI MISO                |
| SD Card SCK                    | 52 (Mega) / 13 (Uno) | SPI SCK                 |

## Power Consumption

-   **Sleep Mode**: ~50μA (with external RTC)
-   **Active Mode**: ~100mA during sensor readings
-   **Sleep Duration**: 1 hour between readings
-   **Estimated Battery Life**: 3-6 months with 9V battery

## Operation Cycle

1. **Wake Up**: Arduino wakes from sleep via RTC interrupt
2. **Initialize**: Set up sensors and SD card
3. **Sensor Readings**: Wake sensors, take readings, sleep sensors
4. **Data Storage**: Write timestamp and sensor data to SD card
5. **Sleep**: Return to sleep mode for 1 hour

## Data Format

Data is stored in `sensor.csv` on the SD card with the following format:

```csv
timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity,turbidity_ntu
2024-01-15 14:30:00,7.25,23.5,8.2,1250.5,2.3
```

## Setup Instructions

### 1. Install Required Libraries

In Arduino IDE, install these libraries:

-   `DS3231` (Real Time Clock)
-   `SD` (SD Card)
-   `SPI` (Serial Peripheral Interface)
-   `Ezo_i2c` (Atlas Scientific)
-   `Ezo_i2c_util` (Atlas Scientific)
-   `sequencer3` and `sequencer4` (Custom sequencer libraries)

### 2. Hardware Assembly

1. Connect DS3231 RTC module to I2C pins (A4, A5)
2. Connect SD card module to SPI pins
3. Connect Atlas Scientific sensors to I2C bus
4. Connect turbidity sensor to analog pin A1
5. Connect interlink control pins (3, 4)

### 3. Upload Code

-   Upload the `aquas-sensor-buoy.ino` file
-   The Arduino will automatically create the CSV file on first run

### 4. Monitor Output

Connect to Serial Monitor (9600 baud) to see:

-   System initialization
-   Sensor readings
-   Data storage confirmation
-   Sleep cycle information

## Key Features

### Sequencer System

The code uses a custom sequencer system to manage the sensor reading cycle:

-   `step1()`: Wake sensors and send read commands
-   `step2()`: Receive sensor readings and send EC temperature compensation
-   `step3()`: Receive EC reading, read turbidity, and save data
-   `sleepStep()`: Prepare for sleep mode

### Power Management

-   External RTC provides precise timing
-   Sleep mode between readings for power efficiency
-   Sensor sleep commands to minimize power consumption

### Data Storage

-   SD card provides reliable data storage
-   CSV format for easy data analysis
-   Automatic file creation with headers

## Troubleshooting

### Common Issues

1. **SD Card Not Detected**

    - Check SPI connections
    - Verify chip select pin configuration
    - Ensure SD card is properly formatted (FAT32)

2. **RTC Not Working**

    - Check I2C connections
    - Verify RTC module is powered
    - Check interrupt pin connection

3. **Sensor Communication Issues**

    - Check I2C connections (A4, A5)
    - Verify sensor addresses
    - Ensure proper power supply to sensors

4. **Sleep Mode Problems**
    - Check interrupt pin configuration
    - Ensure no floating pins
    - Verify power supply during sleep

## Customization

### Change Sleep Duration

```cpp
// In setup() function
rtc.setAlarm1(0, 0, 0, 0, DS3231_MATCH_M_S); // Every minute
```

### Modify Sensor Addresses

```cpp
Ezo_board PH = Ezo_board(99, "PH"); // Change 99 to your sensor's address
```

### Change Data Collection Frequency

```cpp
// Modify sequencer timing in setup()
Sequencer4 readSequence(&step1, 1000, &step2, 1000, &step3, 1000, &sleepStep, 1000);
```

## Limitations

1. **Power Consumption**: Higher than ESP32 version
2. **External Components**: Requires SD card and RTC modules
3. **Limited Storage**: SD card capacity dependent
4. **Complex Wiring**: Multiple external components

## Migration to ESP32

For improved power efficiency and simplified hardware, consider migrating to the ESP32 version located in the `esp32-version/` directory.

## License

This project is the original Arduino implementation of the Aquas Sensor Buoy system.
