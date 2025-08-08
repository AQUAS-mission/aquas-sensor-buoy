#include <Ezo_i2c.h>
#include <Wire.h>
#include <Ezo_i2c_util.h>
#include <SPIFFS.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <DS3231.h>
#include <WiFi.h>

// Interlink isolated channel disable pin. HIGH = disable
const int interlinkIsolatedDisablePin = 3;
// Interlink non-isolated channel disable pin. LOW = disable
const int interlinkNonIsolatedDisablePin = 4;

// Turbidity sensor pin (ADC1_CH6 on GPIO34)
const int turbidityPin = 34;

// Sleep duration in microseconds (1 hour = 3600000000 microseconds)
const uint64_t SLEEP_DURATION = 3600000000ULL; // 1 hour

String filename = "/sensor.csv";

// RTC object
DS3231 rtc;

// ****************************************
// EZO interlink sensor configuration
Ezo_board DO = Ezo_board(97, "DO");     //dissolved oxygen
Ezo_board PH = Ezo_board(99, "PH");     //ph
Ezo_board EC = Ezo_board(100, "EC");    //electrical conductivity
Ezo_board RTD = Ezo_board(102, "RTD");  //temperature
// ****************************************

// ****************************************
// SENSOR MEMORY MANAGEMENT

// Single buffers to hold the current sensor readings
char ph_receive_buffer[32];
char rtd_receive_buffer[32];
char do_receive_buffer[32];
char ec_receive_buffer[32];

// Turbidity reading variable
float turbidityNTU = 0.0;

// ADC calibration
esp_adc_cal_characteristics_t adc_chars;

// Overall system error code (0 = success)
// Error Code System:
// 0 = Success (all sensors working)
// 1 = RTC error (time not available)
// 2 = pH sensor error
// 3 = RTD (temperature) sensor error  
// 4 = DO (dissolved oxygen) sensor error
// 5 = EC (electrical conductivity) sensor error
// 6 = Turbidity sensor error
// 7 = Multiple errors present
int system_error_code = 0;

// ****************************************

// Function to read turbidity from the sensor, based on temperature compensation and conversion from voltage to NTU
float readTurbidity(float temperature) {
  // Read analog value from turbidity sensor
  int sensorValue = analogRead(turbidityPin);

  // Convert to voltage (ESP32 ADC is 12-bit, 0-3.3V)
  float voltage = sensorValue * (3.3 / 4095.0);

  // Convert voltage to NTU using quadratic formula for 3.3V operation
  // Based on DFRobot SEN0189 calibration adjusted for 3.3V: y = -1120.4x² + 5742.3x - 4352.9
  // Adjusted for 3.3V instead of 5V operation
  float ntu = -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;

  // Apply temperature compensation: turbidity readings typically increase by ~2% per °C above 20°C
  float tempCompensation = 1.0 + 0.02 * (temperature - 20.0);
  ntu = ntu / tempCompensation;

  // Ensure NTU is not negative
  if (ntu < 0) {
    ntu = 0;
  }

  return ntu;
}

void initSPIFFS() {
  // Initialize SPIFFS
  Serial.print("Initializing SPIFFS...");
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS failed to mount");
    while (1);
  }
  Serial.println("SPIFFS initialized.");

  delay(100);

  // Create CSV file with headers if it doesn't exist
  if (!SPIFFS.exists(filename)) {
    Serial.println("File doesn't exist, creating new file...");
    File dataFile = SPIFFS.open(filename, FILE_WRITE);
    if (dataFile) {
      Serial.println("File opened successfully, writing headers...");
      dataFile.println("timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity,turbidity_ntu,error_code");
      dataFile.close();
      Serial.println("Created new CSV file with headers");
    } else {
      Serial.println("Error creating CSV file");
    }
  } else {
    Serial.println("CSV file already exists");
  }
}

void disableUnnecessaryPeripherals() {
  // Disable WiFi
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();
  
  // Disable Bluetooth
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  
  // Note: adc2_power_release() is not available in all ESP32 Arduino versions
  // Removing this call as it's not essential for power saving
  // adc2_power_release();
  
  Serial.println("Unnecessary peripherals disabled");
}

void setupTime() {
  // Initialize RTC
  rtc.begin();
  
  // Set time to compile time if RTC is not running
  // Note: DS3231 library for ESP32 may have different API
  // Using RTCDateTime instead of DateTime
  RTCDateTime dt = rtc.getDateTime();
  if (dt.year < 2020) {
    Serial.println("RTC is NOT running, setting to compile time!");
    rtc.setDateTime(__DATE__, __TIME__);
  }
  
  // Check if RTC is working properly
  dt = rtc.getDateTime();
  if (dt.year < 2020) {
    system_error_code = 1; // RTC error
    Serial.println("Error: RTC not working properly");
  } else {
    Serial.printf("Current time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                  dt.year, dt.month, dt.day,
                  dt.hour, dt.minute, dt.second);
  }
}

void setup() {
  // Initialize Serial first for debugging
  Serial.begin(9600);
  Serial.println("ESP32 Sensor Buoy Starting...");
  
  // Set up time BEFORE disabling WiFi
  setupTime();
  
  // Disable unnecessary peripherals after time sync
  disableUnnecessaryPeripherals();
  
  // Set up pins
  pinMode(interlinkIsolatedDisablePin, OUTPUT);
  pinMode(interlinkNonIsolatedDisablePin, OUTPUT);
  
  // Initialize I2C
  Wire.begin();
  
  // Initialize SPIFFS
  initSPIFFS();
  
  // Initialize ADC for turbidity sensor
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // 0-3.3V range
  
  // Calibrate ADC
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  
  // Wake up interlink channels
  wakeInterlinkChannels();
  
  Serial.println("System ready - data will be saved to " + filename);
  
  // Take sensor readings
  takeSensorReadings();
  
  // Go to deep sleep
  goToDeepSleep();
}

void loop() {
  // This should never be reached in normal operation
  // The ESP32 will restart after deep sleep and run setup() again
}

void takeSensorReadings() {
  Serial.println("Taking sensor readings...");
  
  // Wake sensors and delay for time until stable reading
  wakeSensors();
  
  // Step 1: Send read commands to DO, PH, and RTD
  Serial.println("Step 1: Sending read commands...");
  DO.send_read_cmd();
  PH.send_read_cmd();
  RTD.send_read_cmd();
  
  // Step 2: Wait for and receive readings from DO, PH, RTD
  Serial.println("Step 2: Receiving readings from DO, PH, RTD...");
  
  // Wait for DO sensor to be ready
  while (DO.get_error() == Ezo_board::NOT_READY) {
    delay(100);
  }
  enum Ezo_board::errors do_err = DO.receive_cmd(do_receive_buffer, 32);
  
  // Wait for PH sensor to be ready
  while (PH.get_error() == Ezo_board::NOT_READY) {
    delay(100);
  }
  enum Ezo_board::errors ph_err = PH.receive_cmd(ph_receive_buffer, 32);
  
  // Wait for RTD sensor to be ready
  while (RTD.get_error() == Ezo_board::NOT_READY) {
    delay(100);
  }
  enum Ezo_board::errors rtd_err = RTD.receive_cmd(rtd_receive_buffer, 32);

  // Check for sensor errors and set system error code
  int error_count = 0;
  
  if (ph_err != Ezo_board::SUCCESS) {
    Serial.println("Warning: pH sensor communication error");
    system_error_code = 2; // pH sensor error
    error_count++;
  }
  if (rtd_err != Ezo_board::SUCCESS) {
    Serial.println("Warning: RTD sensor communication error");
    system_error_code = 3; // RTD sensor error
    error_count++;
  }
  if (do_err != Ezo_board::SUCCESS) {
    Serial.println("Warning: DO sensor communication error");
    system_error_code = 4; // DO sensor error
    error_count++;
  }

  // Step 3: Send EC command with temperature compensation
  Serial.println("Step 3: Sending EC command with temperature compensation...");
  if ((RTD.get_error() == Ezo_board::SUCCESS) && (RTD.get_last_received_reading() > -1000.0)) {
    EC.send_read_with_temp_comp(RTD.get_last_received_reading());
    Serial.println("EC command sent with temperature compensation");
  } else {
    // Default case: EC with 25˚C default temperature
    EC.send_read_with_temp_comp(25.0);
    Serial.println("EC command sent with default temperature (25°C)");
  }
  
  // Step 4: Wait for and receive EC reading
  Serial.println("Step 4: Receiving EC reading...");
  while (EC.get_error() == Ezo_board::NOT_READY) {
    delay(100);
  }
  enum Ezo_board::errors ec_err = EC.receive_cmd(ec_receive_buffer, 32);
  
  if (ec_err != Ezo_board::SUCCESS) {
    Serial.println("Warning: EC sensor communication error");
    system_error_code = 5; // EC sensor error
    error_count++;
  }

  // Step 5: Read turbidity with temperature compensation
  Serial.println("Step 5: Reading turbidity sensor...");
  float temperature = RTD.get_last_received_reading();
  if (temperature <= -1000.0) {
    temperature = 25.0;  // Default temperature if RTD reading failed
    Serial.println("Using default temperature (25°C) for turbidity compensation");
  }
  turbidityNTU = readTurbidity(temperature);
  
  // Check for turbidity sensor error (out of range readings)
  if (turbidityNTU < 0 || turbidityNTU > 3000) {
    system_error_code = 6; // Turbidity sensor error
    Serial.println("Warning: Turbidity reading out of range");
    error_count++;
  }

  // Check for multiple errors and set error code to 7 if multiple sensors failed
  if (error_count > 1) {
    system_error_code = 7; // Multiple errors present
    Serial.printf("Warning: Multiple sensor errors detected (%d sensors failed)\n", error_count);
  }

  // Write data to SPIFFS
  writeDataToFile();
  
  // Sleep sensors
  sleepSensors();
}

void writeDataToFile() {
  File dataFile = SPIFFS.open(filename, FILE_APPEND);

  if (dataFile) {
    // Get current timestamp from RTC
    RTCDateTime dt = rtc.getDateTime();
    
    // Format timestamp as YYYY-MM-DD HH:MM:SS
    char timestamp[64];
    snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             dt.year, dt.month, dt.day,
             dt.hour, dt.minute, dt.second);

    // Write timestamp
    dataFile.print(timestamp);
    dataFile.print(",");

    // Write sensor data
    dataFile.print(ph_receive_buffer);
    dataFile.print(",");
    dataFile.print(rtd_receive_buffer);
    dataFile.print(",");
    dataFile.print(do_receive_buffer);
    dataFile.print(",");
    dataFile.print(ec_receive_buffer);
    dataFile.print(",");
    dataFile.print(turbidityNTU, 2);  // Print with 2 decimal places
    dataFile.print(",");
    dataFile.print(system_error_code);  // Print error code
    dataFile.println();

    dataFile.close();
    Serial.println("Data saved to SPIFFS");
    
    // Print to Serial for debugging
    Serial.print("Timestamp: "); Serial.println(timestamp);
    Serial.print("pH: "); Serial.println(ph_receive_buffer);
    Serial.print("Temperature: "); Serial.println(rtd_receive_buffer);
    Serial.print("DO: "); Serial.println(do_receive_buffer);
    Serial.print("EC: "); Serial.println(ec_receive_buffer);
    Serial.print("Turbidity: "); Serial.println(turbidityNTU, 2);
    Serial.print("Error Code: "); Serial.println(system_error_code);
    
  } else {
    Serial.println("Error opening " + filename + " for writing");
    // Fallback to Serial output if SPIFFS fails
    Serial.println("Fallback - printing to Serial:");
    Serial.print(ph_receive_buffer);
    Serial.print(";");
    Serial.print(rtd_receive_buffer);
    Serial.print(";");
    Serial.print(do_receive_buffer);
    Serial.print(";");
    Serial.print(ec_receive_buffer);
    Serial.print(";");
    Serial.print(turbidityNTU, 2);
    Serial.print(";");
    Serial.print(system_error_code);
    Serial.println();
  }
}

void goToDeepSleep() {
  Serial.println("Preparing for deep sleep...");
  sleepSensors();  // Sleep all our sensors
  sleepInterlinkChannels();
  Serial.flush();  // Ensure all serial data is sent

  Serial.printf("Going to deep sleep for %llu microseconds (%llu hours)\n", 
                SLEEP_DURATION, SLEEP_DURATION / 3600000000ULL);
  
  // Configure deep sleep
  esp_sleep_enable_timer_wakeup(SLEEP_DURATION);
  
  // Go to deep sleep
  esp_deep_sleep_start();
  
  // This line should never be reached
  Serial.println("This should never be printed");
}

// Sleep all sensors indefinitely until any other command is issued.
void sleepSensors() {
  char* sleepCommand = "Sleep";
  DO.send_cmd(sleepCommand);
  PH.send_cmd(sleepCommand);
  EC.send_cmd(sleepCommand);
  RTD.send_cmd(sleepCommand);
  Serial.println("ALL sensors sleeping...");
}

// Send an arbitrary command to wake all sensors + delay
void wakeSensors() {
  char* wakeCommand = "Status";
  DO.send_cmd(wakeCommand);
  PH.send_cmd(wakeCommand);
  EC.send_cmd(wakeCommand);
  RTD.send_cmd(wakeCommand);
  Serial.println("All sensors waking, delaying for 20s...");
  delay(20000);
}

void sleepInterlinkChannels() {
  digitalWrite(interlinkNonIsolatedDisablePin, LOW);
  digitalWrite(interlinkIsolatedDisablePin, HIGH);
  Serial.println("Sleeping interlink channels...");
}

void wakeInterlinkChannels() {
  digitalWrite(interlinkNonIsolatedDisablePin, HIGH);
  digitalWrite(interlinkIsolatedDisablePin, LOW);
  Serial.println("Waking interlink channels...");
} 