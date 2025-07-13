Research notes on sleeping the Arduino and Raspi:

The RasPi does not have a timed sleep mode, but the Arduino does. Currently exploring: controlling PI wake/sleep mode via the Arduino, which is set to turn on every X minutes.

On the Pi, it isn’t possible to run anything in its low power mode, since it is basically equivalent to be completely off.

**The following is relevant for more advanced Arduino models, not the ones we are using.**

To manage power consumption in the Arduino, the main/most effective method is putting the Arduino in sleep mode. In sleep mode, no code can be run. Sleep can be interrupted, however, by the following methods:

-   **Deep Sleep**: the Arduino can be set to wake up after a set time. This is most power efficient, since it is put in Deep Sleep mode, which uses the least possible amount of power. In this mode, there are very limited possibilities of running peripherals. “This will stop every clock sources of the microcontroller and set the voltage regulators to be in low power state. Oscillators can be in 3 different states where it stops or run, and run on behalf of peripheral request. The device will be then in deep sleep while WFI (Wait For Interrupt) is active.”
-   (Non-Deep) Sleep: (Idle Mode): Continue running peripherals. In this mode, it’s possible to wake up based on set external events, including by a change in voltage from a connected device.
-   Unless it is necessary to react to external changes independent from time, it seems that Deep Sleep/waking based on times is appropriate here.

**For our Arduino Uno R3s:**

-   Can only sleep in 8s intervals! See below
-   https://github.com/rocketscream/Low-Power/issues/98
-   The official ArduinoLowPower library does not work, instead, https://github.com/rocketscream/Low-Power seems to work well. Find it in the Arduino IDE as Low-Power by Rocket Scream Electronics
-   How efficient this is (sleeping in 8s intervals) is unknown—todo test power draw.

Advanced Linux modifications (from Nick):

-   Software won’t start running again, and will be at standby.
-   Might have to reboot software tasks after the RasPi switches back on. Bash script to start processes?
-   Kill processes in Linux to shut down specific processes.

https://littlebirdelectronics.com.au/blogs/news/how-can-i-sleep-a-raspberry-pi-and-wake-it-again-with-an-interrupt?srsltid=AfmBOop3bE5hew3rB6RGzLl3DPPBmlCIu-EkME9XCSxDgSa9bP72l3IG

https://docs.arduino.cc/learn/electronics/low-power/

---

# Arduino Water Quality Sensor System

## Overview

The `aquas-arduino-sensors.ino` file implements a multi-sensor water quality monitoring system that:

-   Reads pH, temperature (RTD), dissolved oxygen (DO), electrical conductivity (EC), and turbidity sensors
-   Logs data to an SD card in CSV format with timestamps
-   Uses RTC-based sleep/wake cycles for power management
-   Provides temperature compensation for sensor readings

## Required Libraries

To compile and run this code, you need to install the following libraries in your Arduino IDE. Most of these are included as folders in this repository:

### Core Libraries (included in repo folders):

1. **DS3231** - Real-time clock functionality
2. **Ezo_i2c** - Communication with Atlas Scientific EZO sensors
3. **sequencer3** & **sequencer4** - State machine for sensor reading sequence
4. **Ezo_i2c_util** - Utility functions for EZO sensors

### Standard Arduino Libraries (install via Library Manager):

1. **Wire** - I2C communication (built-in)
2. **SD** - SD card operations (built-in)
3. **SPI** - SPI communication for SD card (built-in)
4. **RTClib** - Additional RTC functionality

### Installation Instructions:

1. Copy the library folders from this repo to your Arduino libraries directory:

    - Windows: `Documents/Arduino/libraries/`
    - Mac: `Documents/Arduino/libraries/`
    - Linux: `~/Arduino/libraries/`

2. Install RTClib via Arduino IDE:
    - Go to `Sketch > Include Library > Manage Libraries`
    - Search for "RTClib" by Adafruit
    - Click Install

## Hardware Setup

### Sensor Connections:

-   **pH Sensor**: I2C address 99
-   **Temperature (RTD)**: I2C address 102
-   **Dissolved Oxygen (DO)**: I2C address 97
-   **Electrical Conductivity (EC)**: I2C address 100
-   **Turbidity Sensor**: Analog pin A1
-   **RTC (DS3231)**: I2C connection + interrupt pin 2
-   **SD Card Module**: SPI connection (CS pin 53 for Mega, pin 10 for Uno)

### Power Requirements:

-   5V power supply recommended
-   Turbidity sensor requires 5V operation
-   EZO sensors can operate on 3.3V or 5V

## How It Works

### 1. System Architecture

The system uses a 4-step sequencer pattern:

-   **Step 1**: Send read commands to DO, pH, and RTD sensors
-   **Step 2**: Receive responses + send EC command with temperature compensation
-   **Step 3**: Receive EC response + read turbidity + write all data to SD card
-   **Step 4**: Sleep management and wake cycle control

### 2. Data Flow

```
Sensors → I2C/Analog Read → Temperature Compensation → SD Card CSV → Sleep
```

### 3. Key Functions

#### `setup()`

-   Initializes RTC with 3-hour alarm intervals
-   Sets up sleep mode configuration
-   Initializes SD card and creates CSV file with headers
-   Configures I2C and sensor pins

#### `step1()` - Command Phase

-   Sends read commands to pH, DO, and RTD sensors simultaneously
-   Non-blocking - sensors process readings in background

#### `step2()` - Response + EC Setup

-   Receives responses from pH, DO, and RTD sensors
-   Uses RTD temperature reading for EC temperature compensation
-   Sends temperature-compensated read command to EC sensor

#### `step3()` - Final Data Collection

-   Receives EC sensor response
-   Reads turbidity sensor with temperature compensation
-   Writes complete sensor reading to SD card with timestamp
-   Handles SD card errors with Serial fallback

#### `sleepStep()` - Power Management

-   Manages sleep/wake cycles based on RTC time
-   Prevents redundant readings within the same minute
-   Controls when to take readings vs. sleep

### 4. Data Format

CSV file (`sensor.csv`) contains:

```
timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity,turbidity_ntu
2024-01-15 14:30:25,7.2,23.5,8.1,1250,12.45
```

### 5. Temperature Compensation

-   **EC Sensor**: Uses RTD reading for automatic temperature compensation
-   **Turbidity**: Applies 2% correction per °C deviation from 20°C reference
-   **Fallback**: Uses 25°C default if RTD reading fails

### 6. Error Handling

-   SD card failure → Serial output fallback
-   Sensor communication errors → Logged in data
-   RTC issues → System continues with basic timing
-   Negative turbidity values → Clamped to 0

### 7. Power Management

-   Uses DS3231 RTC alarms for wake events
-   Arduino sleep mode between readings
-   Configurable wake intervals (currently every 2 minutes for testing)
-   Prevents redundant readings within same wake cycle

## Troubleshooting

### Common Issues:

1. **SD Card Problems**: Ensure FAT32 formatting and proper wiring
2. **Sensor Communication**: Check I2C addresses and connections
3. **Sleep Issues**: Verify RTC wiring and interrupt pin connection
4. **Memory Issues**: Code optimized for Arduino Uno (2KB RAM limit)

### Debug Output:

The system provides detailed Serial output for monitoring:

-   Sensor initialization status
-   SD card operations
-   Sleep/wake cycles
-   Error conditions
