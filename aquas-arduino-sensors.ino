#include <DS3231.h>
#include <Ezo_i2c.h>
#include <Wire.h>
#include <sequencer3.h>
#include <sequencer4.h>
#include <Ezo_i2c_util.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <SD.h>
#include <SPI.h>

// #include "LowPower.h"

// interrupt pin used for waking the arduino
const int intPin = 2;

// Pins for SD Card module
const int chipSelect = 53; // Use pin 10 for Uno/Nano, change to 53 for Mega

// Turbidity sensor pin
const int turbidityPin = A1; // Analog pin for turbidity sensor
 
String filename = "sensor.csv";

//RTC 
DS3231 rtc;

// ****************************************
// EZO interlink sensor configuraiton
Ezo_board DO = Ezo_board(97, "DO"); //dissolved oxygen
Ezo_board PH = Ezo_board(99, "PH"); //ph
Ezo_board EC = Ezo_board(100, "EC"); //electrical conductivity
Ezo_board RTD = Ezo_board(102, "RTD"); //temperature
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

// ****************************************

// callback called upon the arduino waking
// can be empty, but not null
void wakeup(){
}

// steps of sensor readings and sleeping
void step1();
void step2();
void step3();
void sleepStep();
void goToSleep();

bool takenReadingThisWakeCycle = false;

//Last number here is probably the delay (in ms) after step 3. Adjust this time to be appropriate amount for a 15min delay.
//NOTE: This is a temporary solution, as it doesn't let the Arduino sleep. To allow for sleep (using Arduino Mega):
//Revert the number here to a low value (eg 1000) and sleep within Step 3 for 15mins via the Arduino sleep module:
//#include "ArduinoLowPower.h" // AND
//LowPower.sleep(10000); //this value is incorrect, adjust to 15 mins
//^^^^^include this in step 3
//https://docs.arduino.cc/learn/electronics/low-power/
Sequencer4 readSequence(&step1, 1000, &step2, 1000, &step3, 1000, &sleepStep, 1000);

// Function to read turbidity from the sensor, based on temperature compensation and conversion from voltage to NTU
float readTurbidity(float temperature) {
  // Read analog value from turbidity sensor
  int sensorValue = analogRead(turbidityPin);
  
  // Convert to voltage (assuming 5V Arduino)
  float voltage = sensorValue * (5.0 / 1024.0);
  
  // Convert voltage to NTU using quadratic formula for 5V operation
  // Based on DFRobot SEN0189 calibration: y = -1120.4x² + 5742.3x - 4352.9
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

void initSD() {
  // Initialize SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // Don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");
  
  delay(100);
  
  // Create CSV file with headers if it doesn't exist
  if (!SD.exists(filename)) {
    Serial.println("File doesn't exist, creating new file...");
    File dataFile = SD.open(filename, FILE_WRITE);
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

void setup() {
  //set up real time clock (RTC) DS3231
  rtc.begin();
  rtc.setDateTime(__DATE__, __TIME__);
  rtc.setAlarm1(0, 0, 0, 0, DS3231_MATCH_S); //sets alarm for every minute
  rtc.enableOutput(false);

  //set time if it hasn;t been set yet
  // will set to the time which the sketch was compiled
  // if (! rtc.isrunning()) {
  //   Serial.println("RTC is NOT running, let's set the time!");
  //   // When time needs to be set on a new device, or after a power loss, the
  //   // following line sets the RTC to the date & time this sketch was compiled
  //   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // }

  //set arduino sleep method
  pinMode(intPin, INPUT);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // Initialize turbidity sensor pin
  pinMode(turbidityPin, INPUT);

  Wire.begin();
  Serial.begin(9600);
  Serial.println("In setup");
  // Initialize SD card
  initSD();
  
  readSequence.reset();
  Serial.println("System ready - data will be saved to " + filename);
}

void loop() {
  readSequence.run();
}

void step1(){
  // send a read command
  DO.send_read_cmd();
  PH.send_read_cmd();
  RTD.send_read_cmd();
}

void step2(){
  enum Ezo_board::errors myerr = PH.receive_cmd(ph_receive_buffer,32);
  RTD.receive_cmd(rtd_receive_buffer,32);
  DO.receive_cmd(do_receive_buffer,32);

  if ((RTD.get_error() == Ezo_board::SUCCESS) && (RTD.get_last_received_reading() > -1000.0)) {
    EC.send_read_with_temp_comp(RTD.get_last_received_reading());
  } else {
    // Default case: EC with 25˚C default temperature
    EC.send_read_with_temp_comp(25.0);
  }
}

void step3(){
  EC.receive_cmd(ec_receive_buffer,32);

  // Read turbidity with temperature compensation
  float temperature = RTD.get_last_received_reading();
  if (temperature <= -1000.0) {
    temperature = 25.0; // Default temperature if RTD reading failed
  }
  turbidityNTU = readTurbidity(temperature);

  // Now we have all sensor readings - write immediately to SD card
  File dataFile = SD.open(filename, FILE_WRITE);
  
  if (dataFile) {
    // Get current timestamp
    RTCDateTime dt = rtc.getDateTime();
    
    // Write timestamp
    dataFile.print(rtc.dateFormat("Y-m-d H:i:s", dt));
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
    dataFile.print(turbidityNTU, 2); // Print with 2 decimal places
    dataFile.println();
    
    dataFile.close();
    Serial.println("Data saved to SD card");
  } else {
    Serial.println("Error opening " + filename + " for writing");
    // Fallback to Serial output if SD card fails
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
  takenReadingThisWakeCycle = true;
}

void goToSleep(){
    Serial.println("Preparing for sleep...");
    Serial.flush(); // Ensure all serial data is sent before clock change
    
    // Lower clock speed to save power (16MHz -> 2MHz)
    clock_prescale_set(clock_div_8); // Divide by 8
    
    //sleeping, to be woken by interrupt pin
    attachInterrupt(digitalPinToInterrupt(intPin), wakeup, LOW);
    delay(50); // Shorter delay due to slower clock
    sleep_cpu();

    //waking
    detachInterrupt(digitalPinToInterrupt(intPin));
    rtc.clearAlarm1();
    
    // Restore full clock speed after waking
    clock_prescale_set(clock_div_1); // No division (full 16MHz)
    
    Serial.println("Awake! Clock speed restored.");
}

void sleepStep(){
  RTCDateTime dt = rtc.getDateTime();
  Serial.println(dt.minute);
  Serial.println(dt.minute % 2 != 0);
  while(dt.minute % 2 != 0){
    goToSleep();
    dt = rtc.getDateTime();
  }
  if(takenReadingThisWakeCycle = true) {
    Serial.println("Already taken a reading");
    goToSleep(); //taking another reading this minute is redundant
    dt = rtc.getDateTime();
  }
    Serial.println(rtc.dateFormat("H:i:s", dt));
    delay(100);
    takenReadingThisWakeCycle = false;
}

