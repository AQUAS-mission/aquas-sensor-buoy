#include <Ezo_i2c_esp32.h>
#include <Wire.h>
#include <Ezo_i2c_util_esp32.h>
#include <SPIFFS.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <esp_bt.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include <DS3231.h>
#include <WiFi.h>
#include <sequencer3.h>
#include <sequencer4.h>

// Interlink isolated channel disable pin. HIGH = disable
const int interlinkIsolatedDisablePin = 5;
// Interlink non-isolated channel disable pin. LOW = disable
const int interlinkNonIsolatedDisablePin = 18;

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

// Helper function to convert error codes to readable messages
const char* getErrorString(enum Ezo_board::errors error_code) {
  switch (error_code) {
    case Ezo_board::SUCCESS: return "SUCCESS";
    case Ezo_board::FAIL: return "FAIL";
    case Ezo_board::NOT_READY: return "NOT_READY";
    case Ezo_board::NO_DATA: return "NO_DATA";
    case Ezo_board::NOT_READ_CMD: return "NOT_READ_CMD";
    default: return "UNKNOWN_ERROR";
  }
}

// Sequencer step functions
void step1();
void step2();
void step3();
void sleepStep();

//Last number here is probably the delay (in ms) after step 3. Adjust this time to be appropriate amount for a 15min delay.
//NOTE: This is a temporary solution, as it doesn't let the ESP32 sleep. To allow for sleep (using ESP32):
//Revert the number here to a low value (eg 1000) and sleep within Step 3 for 15mins via the ESP32 sleep module:
//esp_sleep_enable_timer_wakeup(900000000); // 15 minutes in microseconds
//esp_deep_sleep_start();
//^^^^^include this in step 3
Sequencer4 readSequence(&step1, 1000, &step2, 1000, &step3, 1000, &sleepStep, 1000);

// Function to read turbidity from the sensor, based on temperature compensation and conversion from voltage to NTU
float readTurbidity(float temperature) {
  // Read analog value from turbidity sensor
  int sensorValue = analogRead(turbidityPin);

  // Convert to voltage (ESP32 ADC is 12-bit, 0-3.3V)
  float voltage = sensorValue * (3.3 / 4095.0);

  // Convert voltage to NTU using quadratic formula for 3.3V operation
  // Based on DFRobot SEN0189 calibration adjusted for 3.3V: y = -1120.4x² + 5742.3x - 4352.9
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
  WiFi.mode(WIFI_MODE_NULL);
  esp_wifi_stop();
  
  // Disable Bluetooth
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  
  Serial.println("Unnecessary peripherals disabled");
}

void setupTime() {
  // Initialize RTC
  rtc.begin();
  
  // Set time to compile time if RTC is not running
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

// Hardware diagnostic function
void runHardwareDiagnostics() {
  Serial.println("=== HARDWARE DIAGNOSTICS ===");
  
  // Test basic I2C functionality
  Serial.println("Testing basic I2C functionality...");
  
  // Test with a simple I2C scan
  Serial.println("Performing I2C scan...");
  int foundDevices = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("✓ Device found at 0x%02X\n", addr);
      foundDevices++;
    }
  }
  
  Serial.printf("Total I2C devices found: %d\n", foundDevices);
  
  if (foundDevices == 0) {
    Serial.println("❌ NO I2C DEVICES FOUND!");
    Serial.println("This indicates a hardware problem:");
    Serial.println("1. Check power supply to sensors");
    Serial.println("2. Add 4.7kΩ pull-up resistors to SDA/SCL");
    Serial.println("3. Verify wiring connections");
    Serial.println("4. Check sensor power requirements");
  } else if (foundDevices < 4) {
    Serial.printf("⚠️  Only %d devices found (expected 4)\n", foundDevices);
    Serial.println("Some sensors may not be powered or connected");
  } else {
    Serial.println("✓ All 4 sensors detected on I2C bus");
  }
  
  // Test specific sensor addresses
  Serial.println("\nTesting specific sensor addresses...");
  byte addresses[] = {97, 99, 100, 102}; // DO, PH, EC, RTD
  char* names[] = {"DO", "PH", "EC", "RTD"};
  
  for (int i = 0; i < 4; i++) {
    Wire.beginTransmission(addresses[i]);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("✓ %s sensor (0x%02X) responding\n", names[i], addresses[i]);
    } else {
      Serial.printf("✗ %s sensor (0x%02X) NOT responding (error: %d)\n", names[i], addresses[i], error);
    }
    delay(100);
  }
  
  Serial.println("=== END DIAGNOSTICS ===");
}

// Test sensor communication
void testSensorCommunication() {
  Serial.println("=== SENSOR COMMUNICATION TEST ===");
  
  // Wake sensors first
  wakeSensors();
  
  // Test each sensor with Status command
  Serial.println("Testing sensor responses...");
  
  // Test DO sensor
  Serial.println("Testing DO sensor...");
  DO.send_cmd("Status");
  delay(1000);
  char status_buffer[32];
  enum Ezo_board::errors do_status = DO.receive_cmd(status_buffer, 32);
  Serial.printf("DO Status: Error=%d (%s), Response=%s\n", do_status, getErrorString(do_status), status_buffer);
  
  // Test PH sensor
  Serial.println("Testing PH sensor...");
  PH.send_cmd("Status");
  delay(1000);
  enum Ezo_board::errors ph_status = PH.receive_cmd(status_buffer, 32);
  Serial.printf("PH Status: Error=%d (%s), Response=%s\n", ph_status, getErrorString(ph_status), status_buffer);
  
  // Test RTD sensor
  Serial.println("Testing RTD sensor...");
  RTD.send_cmd("Status");
  delay(1000);
  enum Ezo_board::errors rtd_status = RTD.receive_cmd(status_buffer, 32);
  Serial.printf("RTD Status: Error=%d (%s), Response=%s\n", rtd_status, getErrorString(rtd_status), status_buffer);
  
  // Test EC sensor
  Serial.println("Testing EC sensor...");
  EC.send_cmd("Status");
  delay(1000);
  enum Ezo_board::errors ec_status = EC.receive_cmd(status_buffer, 32);
  Serial.printf("EC Status: Error=%d (%s), Response=%s\n", ec_status, getErrorString(ec_status), status_buffer);
  
  Serial.println("=== END COMMUNICATION TEST ===");
}

// Test a complete sensor reading cycle
void testSensorReading() {
  Serial.println("=== SENSOR READING TEST ===");
  
  // Step 1: Send read commands
  Serial.println("Step 1: Sending read commands...");
  DO.send_read_cmd();
  PH.send_read_cmd();
  RTD.send_read_cmd();
  
  // Step 2: Wait and receive readings
  Serial.println("Step 2: Receiving readings...");
  delay(1000); // Wait for sensors to process
  
  enum Ezo_board::errors do_err = DO.receive_cmd(do_receive_buffer, 32);
  enum Ezo_board::errors ph_err = PH.receive_cmd(ph_receive_buffer, 32);
  enum Ezo_board::errors rtd_err = RTD.receive_cmd(rtd_receive_buffer, 32);
  
  Serial.printf("DO: Error=%d (%s), Reading=%s\n", do_err, getErrorString(do_err), do_receive_buffer);
  Serial.printf("PH: Error=%d (%s), Reading=%s\n", ph_err, getErrorString(ph_err), ph_receive_buffer);
  Serial.printf("RTD: Error=%d (%s), Reading=%s\n", rtd_err, getErrorString(rtd_err), rtd_receive_buffer);
  
  // Step 3: Test EC with temperature compensation
  Serial.println("Step 3: Testing EC with temperature compensation...");
  float temp = RTD.get_last_received_reading();
  if (temp > -1000.0) {
    EC.send_read_with_temp_comp(temp);
  } else {
    EC.send_read_with_temp_comp(25.0);
  }
  
  delay(1000);
  enum Ezo_board::errors ec_err = EC.receive_cmd(ec_receive_buffer, 32);
  Serial.printf("EC: Error=%d (%s), Reading=%s\n", ec_err, getErrorString(ec_err), ec_receive_buffer);
  
  // Step 4: Test turbidity
  Serial.println("Step 4: Testing turbidity sensor...");
  float temperature = RTD.get_last_received_reading();
  if (temperature <= -1000.0) temperature = 25.0;
  
  int raw_turbidity = analogRead(turbidityPin);
  turbidityNTU = readTurbidity(temperature);
  Serial.printf("Turbidity: Raw ADC=%d, NTU=%.2f\n", raw_turbidity, turbidityNTU);
  
  Serial.println("=== END READING TEST ===");
  
  // Sleep sensors after test
  sleepSensors();
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
  Serial.println("GPIO pins configured");
  
  // Initialize I2C with proper configuration
  Serial.println("Initializing I2C communication...");
  Wire.begin();
  
  // Configure I2C with proper timing and pull-up settings
  Wire.setTimeOut(5000); // 5 second timeout
  Wire.setClock(100000); // Set to 100kHz for better reliability
  
  Serial.printf("I2C initialized on SDA: %d, SCL: %d\n", SDA, SCL);
  Serial.println("I2C configured: 100kHz clock, 5s timeout");
  
  // Run hardware diagnostics
  runHardwareDiagnostics();
  
  // Initialize SPIFFS
  initSPIFFS();
  
  // Initialize ADC for turbidity sensor
  Serial.println("Initializing ADC for turbidity sensor...");
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // 0-3.3V range
  Serial.printf("ADC configured: 12-bit resolution, 0-3.3V range\n");
  
  // Calibrate ADC
  Serial.println("Calibrating ADC...");
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  Serial.println("ADC calibration complete");
  
  // Wake up interlink channels
  Serial.println("Waking up interlink channels...");
  wakeInterlinkChannels();
  
  // Test sensor communication
  testSensorCommunication();
  
  // Test a complete sensor reading cycle
  testSensorReading();
  
  Serial.println("System ready - data will be saved to " + filename);
  
  // Take sensor readings using sequencer
  takeSensorReadings();
  
  // Go to deep sleep
  goToDeepSleep();
}

void loop() {
  // This should never be reached in normal operation
  // The ESP32 will restart after deep sleep and run setup() again
}

// Steps of sensor readings and sleeping
void step1() {
  // Wake sensors and delay for time until stable reading
  wakeSensors();

  // send a read command
  DO.send_read_cmd();
  PH.send_read_cmd();
  RTD.send_read_cmd();
}

void step2() {
  enum Ezo_board::errors myerr = PH.receive_cmd(ph_receive_buffer, 32);
  RTD.receive_cmd(rtd_receive_buffer, 32);
  DO.receive_cmd(do_receive_buffer, 32);

  if ((RTD.get_error() == Ezo_board::SUCCESS) && (RTD.get_last_received_reading() > -1000.0)) {
    EC.send_read_with_temp_comp(RTD.get_last_received_reading());
  } else {
    // Default case: EC with 25˚C default temperature
    EC.send_read_with_temp_comp(25.0);
  }
}

void step3() {
  EC.receive_cmd(ec_receive_buffer, 32);

  // Read turbidity with temperature compensation
  float temperature = RTD.get_last_received_reading();
  if (temperature <= -1000.0) {
    temperature = 25.0;  // Default temperature if RTD reading failed
  }
  turbidityNTU = readTurbidity(temperature);

  // Now we have all sensor readings - write immediately to SPIFFS
  writeDataToFile();
  
  // Sleep sensors
  sleepSensors();
}

void takeSensorReadings() {
  Serial.println("Taking sensor readings using sequencer...");
  
  readSequence.reset();
  readSequence.run();
  
  Serial.println("Sensor reading sequence complete");
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

void sleepStep() {
  Serial.println("Sleep step completed");
} 