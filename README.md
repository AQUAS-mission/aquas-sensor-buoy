# Aquas Sensor Buoy

A water quality monitoring system that collects sensor data from various water quality sensors and stores the data for analysis. This project provides implementations for both Arduino and ESP32 platforms.

## Project Structure

```
aquas-sensor-buoy/
├── arduino-version/          # Original Arduino implementation
│   ├── aquas-sensor-buoy.ino
│   └── README.md
├── esp32-version/           # ESP32 implementation (recommended)
│   ├── aquas-sensor-buoy-esp32.ino
│   └── README.md
├── Ezo_i2c/                # Atlas Scientific sensor libraries
├── Ezo_i2c_util/
├── sequence_libraries/      # Custom sequencer libraries
├── ezo_sensor_sample/      # Sample sensor code
├── rpi_sleep_test/         # Raspberry Pi sleep testing
└── README.md               # This file
```

## Sensor Configuration

The system monitors the following water quality parameters:

-   **pH**: Atlas Scientific EZO pH sensor (I2C address 99)
-   **Temperature**: Atlas Scientific EZO RTD sensor (I2C address 102)
-   **Dissolved Oxygen**: Atlas Scientific EZO DO sensor (I2C address 97)
-   **Electrical Conductivity**: Atlas Scientific EZO EC sensor (I2C address 100)
-   **Turbidity**: Analog turbidity sensor (DFRobot SEN0189)

## Platform Comparison

| Feature                 | Arduino Version | ESP32 Version  |
| ----------------------- | --------------- | -------------- |
| **Power Efficiency**    | ~50μA sleep     | ~10μA sleep    |
| **Battery Life**        | 3-6 months      | 6-12 months    |
| **External Components** | SD card + RTC   | None required  |
| **Storage**             | SD card         | Built-in flash |
| **Timing**              | External RTC    | Built-in timer |
| **Complexity**          | High            | Low            |
| **Cost**                | Higher          | Lower          |

## Quick Start

### For ESP32 (Recommended)

```bash
cd esp32-version/
# Upload aquas-sensor-buoy-esp32.ino to your ESP32
# See esp32-version/README.md for detailed instructions
```

### For Arduino

```bash
cd arduino-version/
# Upload aquas-sensor-buoy.ino to your Arduino
# See arduino-version/README.md for detailed instructions
```

## Data Collection

Both versions collect data every hour and store it in CSV format:

```csv
timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity,turbidity_ntu
2024-01-15 14:30:00,7.25,23.5,8.2,1250.5,2.3
```

## Power Management

-   **Sleep Duration**: 1 hour between readings
-   **Wake-up**: Automatic timer-based wake-up
-   **Sensor Management**: Sensors are put to sleep between readings
-   **Power Optimization**: All unnecessary peripherals disabled

## Hardware Requirements

### ESP32 Version (Recommended)

-   ESP32 development board
-   Atlas Scientific EZO sensors
-   Turbidity sensor
-   Power supply (3.7V LiPo recommended)

### Arduino Version

-   Arduino Mega 2560 (recommended)
-   SD card module
-   DS3231 RTC module
-   Atlas Scientific EZO sensors
-   Turbidity sensor
-   Power supply (9V battery or external supply)

## Development

### Adding New Sensors

1. Add sensor object in the sensor configuration section
2. Include sensor reading in the measurement sequence
3. Add data column to CSV output
4. Update documentation

### Modifying Collection Frequency

-   **ESP32**: Change `SLEEP_DURATION` constant
-   **Arduino**: Modify RTC alarm settings

### Data Analysis

The CSV output can be analyzed using:

-   Python pandas
-   R statistical analysis
-   Excel/LibreOffice Calc
-   Custom data visualization tools

## Troubleshooting

### Common Issues

1. **Sensor Communication**: Check I2C connections and addresses
2. **Power Issues**: Verify power supply and sleep mode configuration
3. **Data Storage**: Check file system initialization and permissions
4. **Timing Issues**: Verify RTC (Arduino) or internal timer (ESP32) configuration

### Debug Mode

Both versions include Serial output for debugging:

-   Arduino: 9600 baud
-   ESP32: 115200 baud

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## License

This project is open source. See individual version directories for specific licensing information.

## Support

For issues and questions:

1. Check the troubleshooting sections in version-specific READMEs
2. Review the code comments for configuration options
3. Test with individual sensors before full system deployment

## Acknowledgments

-   Atlas Scientific for sensor libraries and documentation
-   ESP32 community for power optimization techniques
-   Arduino community for sensor interfacing examples
