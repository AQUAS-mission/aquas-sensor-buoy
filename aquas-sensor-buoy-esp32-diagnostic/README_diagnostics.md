# ESP32 Sensor Buoy Diagnostics

This document explains how to use the diagnostic system for the ESP32 Sensor Buoy.

## Overview

The diagnostic functions have been separated from the main code into `diagnostics.ino` to keep the main file clean and focused on production operation.

## Files

-   **`aquas-sensor-buoy-esp32.ino`** - Main production code (clean, no diagnostics)
-   **`diagnostics.ino`** - All diagnostic and testing functions
-   **`README_diagnostics.md`** - This file

## How to Use Diagnostics

### Option 1: Include in Arduino Project (Recommended)

1. **Open Arduino IDE**
2. **Create a new sketch or open existing project**
3. **Add both files to your project:**
    - `aquas-sensor-buoy-esp32.ino` (main file)
    - `diagnostics.ino` (diagnostics file)
4. **Set `DEBUG_MODE = true` in the main file**
5. **Upload to ESP32**

### Option 2: Copy Functions Manually

If you prefer to keep everything in one file, you can copy the diagnostic functions from `diagnostics.ino` back into your main file.

## Diagnostic Functions Available

### `runHardwareDiagnostics()`

-   Scans I2C bus for devices
-   Tests specific sensor addresses (DO, PH, EC, RTD)
-   Provides detailed hardware troubleshooting information

### `testSensorCommunication()`

-   Tests communication with each sensor
-   Sends Status commands and reports responses
-   Helps identify communication issues

### `testSensorReading()`

-   Tests complete sensor reading cycle
-   Reads all sensors in sequence
-   Tests temperature compensation for EC sensor
-   Tests turbidity sensor

### `runAllDiagnostics()`

-   Runs all diagnostics in sequence
-   Comprehensive system test

### `getErrorString()`

-   Helper function to convert error codes to readable messages

## Usage Examples

### Basic Hardware Test

```cpp
void setup() {
  // ... other setup code ...

  if (DEBUG_MODE) {
    runHardwareDiagnostics();
  }
}
```

### Full System Test

```cpp
void setup() {
  // ... other setup code ...

  if (DEBUG_MODE) {
    runAllDiagnostics();
  }
}
```

### Individual Tests

```cpp
void setup() {
  // ... other setup code ...

  if (DEBUG_MODE) {
    // Test only what you need
    runHardwareDiagnostics();
    delay(2000);
    testSensorCommunication();
  }
}
```

## Production vs Development

### Production Deployment

-   Set `DEBUG_MODE = false`
-   Diagnostics are skipped
-   Faster startup, lower power consumption
-   Clean, production-ready code

### Development/Testing

-   Set `DEBUG_MODE = true`
-   Include `diagnostics.ino` in project
-   Full diagnostic output
-   Helpful for troubleshooting

## Troubleshooting

### Common Issues

1. **"Function not declared" errors**

    - Make sure `diagnostics.ino` is included in your Arduino project
    - Check that both files are in the same project folder

2. **I2C communication failures**

    - Run `runHardwareDiagnostics()` to identify hardware issues
    - Check power supply and wiring connections
    - Verify pull-up resistors are present

3. **Sensor reading errors**
    - Use `testSensorCommunication()` to test individual sensors
    - Check sensor power and I2C addresses
    - Verify sensor calibration

### Getting Help

When reporting issues, include:

-   Output from `runHardwareDiagnostics()`
-   Output from `testSensorCommunication()`
-   Any error messages from the main code
-   Hardware setup details

## File Structure

```
aquas-sensor-buoy-esp32/
├── aquas-sensor-buoy-esp32.ino    # Main production code
├── diagnostics.ino                 # Diagnostic functions
├── README_diagnostics.md          # This file
└── ... (other project files)
```

## Benefits of Separation

1. **Clean Main Code**: Production code is focused and easy to read
2. **Modular Design**: Diagnostics can be included/excluded as needed
3. **Easier Maintenance**: Diagnostic code changes don't affect main logic
4. **Production Ready**: Main file can be deployed without diagnostic overhead
5. **Development Friendly**: Easy to enable diagnostics when needed
