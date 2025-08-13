# Ezo_i2c_esp32

ESP32-compatible I2C library for Atlas Scientific EZO sensors.

## Description

This library is an ESP32-compatible version of the Atlas Scientific Ezo_i2c library. It fixes Wire library compatibility issues that occur when using the original library with ESP32 boards.

## Key Changes

-   Fixed `send_cmd()` function to write strings byte-by-byte instead of passing the entire string to `wire->write()`
-   Compatible with ESP32's Wire library which expects `uint8_t` instead of `const char*`
-   All other functionality remains identical to the original library

## Installation

1. Copy this folder to your Arduino libraries directory: `~/Documents/Arduino/libraries/`
2. Restart Arduino IDE
3. The library will appear in the Library Manager

## Usage

```cpp
#include <Ezo_i2c_esp32.h>

// Create sensor objects
Ezo_board PH = Ezo_board(99, "PH");
Ezo_board RTD = Ezo_board(102, "RTD");

// Send commands
PH.send_read_cmd();
delay(1000);

// Receive readings
enum Ezo_board::errors ph_err = PH.receive_read_cmd();
if (ph_err == Ezo_board::SUCCESS) {
  float ph_reading = PH.get_last_received_reading();
  Serial.println(ph_reading);
}
```

## Compatibility

-   ESP32 boards only
-   Atlas Scientific EZO sensors
-   Arduino IDE 1.8.x and 2.x

## License

MIT License - same as original Atlas Scientific library
