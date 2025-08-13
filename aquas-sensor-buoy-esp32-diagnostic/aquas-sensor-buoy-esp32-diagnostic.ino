/*
 * ESP32 Sensor Buoy Diagnostics
 * 
 * This file contains all diagnostic functions for testing and troubleshooting
 * the sensor buoy system. Include this file in your main project when you
 * need to run diagnostics.
 * 
 * Functions included:
 * - runHardwareDiagnostics()
 * - testSensorCommunication() 
 * - testSensorReading()
 * - getErrorString()
 */

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

// Function to run all diagnostics in sequence
void runAllDiagnostics() {
  Serial.println("=== STARTING COMPLETE DIAGNOSTIC SUITE ===");
  
  // Run hardware diagnostics
  runHardwareDiagnostics();
  delay(2000);
  
  // Test sensor communication
  testSensorCommunication();
  delay(2000);
  
  // Test complete sensor reading cycle
  testSensorReading();
  
  Serial.println("=== COMPLETE DIAGNOSTIC SUITE FINISHED ===");
} 