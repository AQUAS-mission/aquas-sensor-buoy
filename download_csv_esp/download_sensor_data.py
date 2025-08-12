#!/usr/bin/env python3
"""
ESP32 Sensor Buoy Data Downloader

This script connects to the ESP32 over serial and downloads the sensor.csv file.
It's designed to work with the aquas-sensor-buoy-esp32 project.

Usage:
    python download_sensor_data.py [port] [baudrate]
    
Examples:
    python download_sensor_data.py                    # Use default settings
    python download_sensor_data.py COM3              # Windows
    python download_sensor_data.py /dev/ttyUSB0      # Linux
    python download_sensor_data.py /dev/tty.usbserial-*  # Mac
    python download_sensor_data.py /dev/ttyUSB0 115200   # Custom baudrate
"""

import serial
import time
import sys
import os
from datetime import datetime

def list_available_ports():
    """List available serial ports"""
    import serial.tools.list_ports
    
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found!")
        return []
    
    print("Available serial ports:")
    for i, port in enumerate(ports):
        print(f"  {i+1}. {port.device} - {port.description}")
    return ports

def select_port_interactively():
    """Let user select a port interactively"""
    ports = list_available_ports()
    if not ports:
        return None
    
    while True:
        try:
            choice = input(f"\nSelect port (1-{len(ports)}) or enter port name: ").strip()
            
            # Check if it's a number (list selection)
            if choice.isdigit():
                idx = int(choice) - 1
                if 0 <= idx < len(ports):
                    return ports[idx].device
                else:
                    print(f"Invalid selection. Please choose 1-{len(ports)}")
                    continue
            
            # Check if it's a valid port name
            if choice in [p.device for p in ports]:
                return choice
            
            # Check if it's a custom port name (might not be in list)
            return choice
            
        except KeyboardInterrupt:
            print("\nCancelled by user")
            return None
        except Exception as e:
            print(f"Error: {e}")

def download_csv_from_esp32(port, baudrate=9600, timeout=30):
    """Download CSV data from ESP32 over serial"""
    
    print(f"Connecting to ESP32 on {port} at {baudrate} baud...")
    
    try:
        # Connect to ESP32
        ser = serial.Serial(port, baudrate, timeout=10)
        time.sleep(2)  # Wait for ESP32 to be ready
        
        print("Connected! Sending dump command...")
        
        # Send dump command
        ser.write(b'dump\n')
        
        # Read response
        csv_data = ""
        start_time = time.time()
        lines_received = 0
        
        print("Downloading data...")
        
        while time.time() - start_time < timeout:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                lines_received += 1
                
                if line == "=== END CSV DUMP ===":
                    print(f"Download complete! Received {lines_received} lines")
                    break
                elif line.startswith("=== CSV DATA DUMP ==="):
                    continue
                elif line.startswith("=== HARDWARE DIAGNOSTICS ==="):
                    print("Skipping diagnostics output...")
                    continue
                elif line.startswith("=== SENSOR COMMUNICATION TEST ==="):
                    print("Skipping communication test output...")
                    continue
                elif line.startswith("=== SENSOR READING TEST ==="):
                    print("Skipping reading test output...")
                    continue
                else:
                    csv_data += line + "\n"
                    
                    # Progress indicator
                    if lines_received % 100 == 0:
                        print(f"  Downloaded {lines_received} lines...")
        
        ser.close()
        
        if not csv_data.strip():
            print("Warning: No CSV data received")
            return None
        
        return csv_data
        
    except serial.SerialException as e:
        print(f"Serial connection error: {e}")
        return None
    except Exception as e:
        print(f"Unexpected error: {e}")
        return None

def save_csv_data(csv_data, filename=None):
    """Save CSV data to a file"""
    
    if filename is None:
        # Generate filename with timestamp
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"sensor_data_{timestamp}.csv"
    
    try:
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(csv_data)
        
        print(f"Data saved to: {filename}")
        return filename
        
    except Exception as e:
        print(f"Error saving file: {e}")
        return None

def display_csv_preview(csv_data, lines=10):
    """Display a preview of the CSV data"""
    print(f"\n=== CSV DATA PREVIEW (first {lines} lines) ===")
    
    lines_data = csv_data.strip().split('\n')
    for i, line in enumerate(lines_data[:lines]):
        print(f"{i+1:2d}: {line}")
    
    if len(lines_data) > lines:
        print(f"... and {len(lines_data) - lines} more lines")
    
    print("=" * 50)

def main():
    """Main function"""
    print("ESP32 Sensor Buoy Data Downloader")
    print("=" * 40)
    
    # Parse command line arguments
    port = None
    baudrate = 9600
    
    if len(sys.argv) >= 2:
        port = sys.argv[1]
    if len(sys.argv) >= 3:
        try:
            baudrate = int(sys.argv[2])
        except ValueError:
            print(f"Invalid baudrate: {sys.argv[2]}")
            return 1
    
    # If no port specified, let user select interactively
    if port is None:
        port = select_port_interactively()
        if port is None:
            print("No port selected. Exiting.")
            return 1
    
    # Validate port
    if not os.path.exists(port) and not port.startswith('COM'):
        print(f"Warning: Port {port} may not exist")
        response = input("Continue anyway? (y/N): ").strip().lower()
        if response != 'y':
            return 1
    
    # Download data
    csv_data = download_csv_from_esp32(port, baudrate)
    
    if csv_data is None:
        print("Failed to download data. Exiting.")
        return 1
    
    # Display preview
    display_csv_preview(csv_data)
    
    # Save data
    filename = save_csv_data(csv_data)
    if filename:
        print(f"\nSuccess! Sensor data downloaded and saved to: {filename}")
        
        # Show file info
        file_size = os.path.getsize(filename)
        print(f"File size: {file_size} bytes")
        
        # Count data rows (excluding header)
        lines = csv_data.strip().split('\n')
        if len(lines) > 1:
            data_rows = len(lines) - 1  # Subtract header
            print(f"Data rows: {data_rows}")
        
        return 0
    else:
        print("Failed to save data.")
        return 1

if __name__ == "__main__":
    try:
        exit_code = main()
        sys.exit(exit_code)
    except KeyboardInterrupt:
        print("\n\nDownload cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        sys.exit(1) 