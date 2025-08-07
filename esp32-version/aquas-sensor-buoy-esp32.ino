#include <Ezo_i2c.h>
#include <Wire.h>
#include <Ezo_i2c_util.h>
#include <SPIFFS.h>
#include <time.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

// Interlink isolated channel disable pin. HIGH = disable
const int interlinkIsolatedDisablePin = 3;
// Interlink non-isolated channel disable pin. LOW = disable
const int interlinkNonIsolatedDisablePin = 4;

// Turbidity sensor pin (ADC1_CH6 on GPIO34)
const int turbidityPin = 34;

// Sleep duration in microseconds (1 hour = 3600000000 microseconds)
const uint64_t SLEEP_DURATION = 3600000000ULL; // 1 hour

String filename = "/sensor.csv";

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
      dataFile.println("timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity,turbidity_ntu");
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
  
  // Disable ADC2 (used by WiFi)
  adc2_power_release();
  
  Serial.println("Unnecessary peripherals disabled");
}

void setupTime() {
  // Set timezone (adjust for your location)
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  // Wait for time to be set
  Serial.println("Waiting for NTP time sync...");
  time_t now = 0;
  struct tm timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 10;
  
  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    Serial.print(".");
    delay(1000);
    time(&now);
    localtime_r(&now, &timeinfo);
  }
  
  if (retry < retry_count) {
    Serial.println("Time synchronized");
  } else {
    Serial.println("Failed to get time, using default");
  }
}

void setup() {
  // Disable unnecessary peripherals first
  disableUnnecessaryPeripherals();
  
  // Set up pins
  pinMode(interlinkIsolatedDisablePin, OUTPUT);
  pinMode(interlinkNonIsolatedDisablePin, OUTPUT);
  
  // Initialize I2C
  Wire.begin();
  
  // Initialize Serial
  Serial.begin(9600);
  Serial.println("ESP32 Sensor Buoy Starting...");
  
  // Initialize SPIFFS
  initSPIFFS();
  
  // Set up time
  setupTime();
  
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
  
  // Step 1: Send read commands
  DO.send_read_cmd();
  PH.send_read_cmd();
  RTD.send_read_cmd();
  
  delay(1000); // Wait for sensors to respond
  
  // Step 2: Receive readings
  enum Ezo_board::errors myerr = PH.receive_cmd(ph_receive_buffer, 32);
  RTD.receive_cmd(rtd_receive_buffer, 32);
  DO.receive_cmd(do_receive_buffer, 32);

  if ((RTD.get_error() == Ezo_board::SUCCESS) && (RTD.get_last_received_reading() > -1000.0)) {
    EC.send_read_with_temp_comp(RTD.get_last_received_reading());
  } else {
    // Default case: EC with 25˚C default temperature
    EC.send_read_with_temp_comp(25.0);
  }
  
  delay(1000); // Wait for EC sensor to respond
  
  // Step 3: Receive EC reading and read turbidity
  EC.receive_cmd(ec_receive_buffer, 32);

  // Read turbidity with temperature compensation
  float temperature = RTD.get_last_received_reading();
  if (temperature <= -1000.0) {
    temperature = 25.0;  // Default temperature if RTD reading failed
  }
  turbidityNTU = readTurbidity(temperature);

  // Write data to SPIFFS
  writeDataToFile();
  
  // Sleep sensors
  sleepSensors();
}

void writeDataToFile() {
  File dataFile = SPIFFS.open(filename, FILE_APPEND);

  if (dataFile) {
    // Get current timestamp
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

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