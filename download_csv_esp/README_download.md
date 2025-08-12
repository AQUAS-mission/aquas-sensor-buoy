# ESP32 Sensor Buoy Data Downloader

This Python script allows you to download sensor data from your ESP32 Sensor Buoy over a serial connection.

## Prerequisites

1. **Python 3.6+** installed on your computer
2. **pyserial** library installed
3. **ESP32 connected** to your computer via USB
4. **ESP32 code uploaded** with the latest version that includes the `dump` command

## Installation

1. Install the required Python library:

    ```bash
    pip install -r requirements.txt
    ```

    Or install manually:

    ```bash
    pip install pyserial
    ```

## Usage

### Basic Usage

```bash
python download_sensor_data.py
```

This will:

-   List available serial ports
-   Let you select a port interactively
-   Connect to the ESP32
-   Download the sensor.csv data
-   Save it to a timestamped file

### Advanced Usage

```bash
# Specify port directly
python download_sensor_data.py COM3                    # Windows
python download_sensor_data.py /dev/ttyUSB0            # Linux
python download_sensor_data.py /dev/tty.usbserial-*    # Mac

# Specify port and baudrate
python download_sensor_data.py /dev/ttyUSB0 115200
```

### Command Line Options

-   **Port**: Serial port name (COM3, /dev/ttyUSB0, etc.)
-   **Baudrate**: Communication speed (default: 9600)

## How It Works

1. **Connects** to ESP32 over serial
2. **Sends** the `dump` command
3. **Receives** CSV data line by line
4. **Filters** out diagnostic output
5. **Saves** clean data to a local file
6. **Shows** preview of the downloaded data

## ESP32 Commands

The ESP32 now responds to these serial commands:

-   **`dump`** - Download the complete sensor.csv file
-   **`help`** - Show available commands
-   **`status`** - Show system status and file info

## Output Files

Data is saved to files with timestamps:

-   `sensor_data_20241201_143022.csv` (format: YYYYMMDD_HHMMSS)

## Troubleshooting

### Common Issues

1. **"No serial ports found"**

    - Check USB connection
    - Install USB-to-serial drivers if needed

2. **"Serial connection error"**

    - Verify correct port name
    - Check if ESP32 is powered on
    - Ensure no other program is using the port

3. **"No CSV data received"**
    - Make sure ESP32 code has the `dump` command
    - Check if sensor.csv file exists on ESP32
    - Verify ESP32 is not in deep sleep

### Port Names by Operating System

-   **Windows**: `COM1`, `COM2`, `COM3`, etc.
-   **Linux**: `/dev/ttyUSB0`, `/dev/ttyUSB1`, etc.
-   **macOS**: `/dev/tty.usbserial-*`, `/dev/tty.usbmodem*`

### Finding Your Port

The script will automatically list available ports. You can also:

-   **Windows**: Device Manager → Ports (COM & LPT)
-   **Linux/macOS**: `ls /dev/tty*` or `ls /dev/cu.*`

## Example Output

```
ESP32 Sensor Buoy Data Downloader
========================================
Available serial ports:
  1. /dev/ttyUSB0 - USB Serial Device
  2. /dev/ttyUSB1 - USB Serial Device

Select port (1-2) or enter port name: 1
Connecting to ESP32 on /dev/ttyUSB0 at 9600 baud...
Connected! Sending dump command...
Downloading data...
Download complete! Received 156 lines

=== CSV DATA PREVIEW (first 10 lines) ===
 1: timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity,turbidity_ntu,error_code
 2: 2024-12-01 14:30:22,7.23,25.16,8.45,1250.67,12.34,0
 3: 2024-12-01 15:30:22,7.21,25.18,8.43,1251.23,11.89,0
...

Data saved to: sensor_data_20241201_143022.csv
File size: 2048 bytes
Data rows: 155

Success! Sensor data downloaded and saved to: sensor_data_20241201_143022.csv
```

## Integration with ESP32 Code

The ESP32 code now includes:

-   Serial command handling in the main loop
-   CSV data dumping functionality
-   Progress indicators for large files
-   Help and status commands

This allows you to download data without interrupting the normal sensor reading cycle.
