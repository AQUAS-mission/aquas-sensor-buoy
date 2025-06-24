#include <DS3231.h>
#include <Ezo_i2c.h>
#include <Wire.h>
#include <sequencer3.h>
#include <sequencer4.h>
#include <Ezo_i2c_util.h>
#include "avr/sleep.h"
#include <SD.h>
#include <SPI.h>

// #include "LowPower.h"

// interrupt pin used for waking the arduino
const int intPin = 2;

// Pins for SD Card module
const int chipSelect = 53; // Use pin 10 for Uno/Nano, change to 53 for Mega
 

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

// ****************************************

// callback called upon the arduino waking
// can be empty, but not null
void wakeup(){
}

// steps of sensor readings and sleeping
void step1();
void step2();
void step3();
void sleeping();

//Last number here is probably the delay (in ms) after step 3. Adjust this time to be appropriate amount for a 15min delay.
//NOTE: This is a temporary solution, as it doesn't let the Arduino sleep. To allow for sleep (using Arduino Mega):
//Revert the number here to a low value (eg 1000) and sleep within Step 3 for 15mins via the Arduino sleep module:
//#include "ArduinoLowPower.h" // AND
//LowPower.sleep(10000); //this value is incorrect, adjust to 15 mins
//^^^^^include this in step 3
//https://docs.arduino.cc/learn/electronics/low-power/
Sequencer4 readSequence(&step1, 1000, &step2, 1000, &step3, 1000, &sleeping, 1000);

void initSD() {
  // Initialize SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // Don't do anything more:
    while (1);
  }
  Serial.println("card initialized.");
  
  // Create CSV file with headers if it doesn't exist
  if (!SD.exists("sensorData.csv")) {
    File dataFile = SD.open("sensorData.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("timestamp,ph,temperature,dissolved_oxygen,electrical_conductivity");
      dataFile.close();
      Serial.println("Created new CSV file with headers");
    } else {
      Serial.println("Error creating CSV file");
    }
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

  Wire.begin();
  Serial.begin(9600);
  Serial.println("In setup");
  // Initialize SD card
  initSD();
  
  readSequence.reset();
  Serial.println("System ready - data will be saved to sensorData.csv");
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

  // Now we have all 4 sensor readings - write immediately to SD card
  File dataFile = SD.open("sensorData.csv", FILE_WRITE);
  
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
    dataFile.println();
    
    dataFile.close();
    Serial.println("Data saved to SD card");
  } else {
    Serial.println("Error opening sensorData.csv for writing");
    // Fallback to Serial output if SD card fails
    Serial.println("Fallback - printing to Serial:");
    Serial.print(ph_receive_buffer);
    Serial.print(";");
    Serial.print(rtd_receive_buffer);
    Serial.print(";");
    Serial.print(do_receive_buffer);
    Serial.print(";");  
    Serial.print(ec_receive_buffer);
    Serial.println();
  }
}

void sleeping(){
  RTCDateTime dt = rtc.getDateTime();
    Serial.println(dt.minute);
    Serial.println(dt.minute % 2 != 0);
  while(dt.minute % 2 != 0){

    //sleeping, to be woken by interrupt pin
    attachInterrupt(digitalPinToInterrupt(intPin), wakeup, LOW);
    Serial.println("sleeping...");
    delay(100);
    sleep_cpu();

    //waking
    detachInterrupt(digitalPinToInterrupt(intPin));
    rtc.clearAlarm1();
    Serial.println("Awake!");
    dt = rtc.getDateTime();
  } 
    Serial.println(rtc.dateFormat("H:i:s", dt));
    delay(100);
}

