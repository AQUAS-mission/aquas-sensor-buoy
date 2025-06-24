#include <DS3231.h>
#include <Ezo_i2c.h>
#include <Wire.h>
#include <sequencer3.h>
#include <sequencer4.h>
#include <Ezo_i2c_util.h>
#include "RTClib.h"
#include "avr/sleep.h"

// #include "LowPower.h"

// interrupt pin used for waking the arduino
const int intPin = 2;


//RTC 
DS3231 rtc;

Ezo_board DO = Ezo_board(97, "DO"); //dissolved oxygen
Ezo_board PH = Ezo_board(99, "PH"); //ph
Ezo_board EC = Ezo_board(100, "EC"); //electrical conductivity
Ezo_board RTD = Ezo_board(102, "RTD"); //temperature

char ph_receive_buffer[32]; 
char rtd_receive_buffer[32]; 
char do_receive_buffer[32]; 
char ec_receive_buffer[32];
// prints <ph>;<temp>;<do>;<ec>


char do_storage[4][32];
char ph_storage[4][32];
char rtd_storage[4][32];
char ec_storage[4][32];

int transmitCounter = 0;

// callback called upon the arduino waking
// can be empty, but not null
void wakeup(){
}

void step1();
void step2();
void step3();

//Last number here is probably the delay (in ms) after step 3. Adjust this time to be appropriate amount for a 15min delay.
//NOTE: This is a temporary solution, as it doesn't let the Arduino sleep. To allow for sleep (using Arduino Mega):
//Revert the number here to a low value (eg 1000) and sleep within Step 3 for 15mins via the Arduino sleep module:
//#include "ArduinoLowPower.h" // AND
//LowPower.sleep(10000); //this value is incorrect, adjust to 15 mins
//^^^^^include this in step 3
//https://docs.arduino.cc/learn/electronics/low-power/
Sequencer3 readSequence(&step1, 1000, &step2, 1000, &step3, 1000);

void setup() {
  //set up real time clock (RTC) DS3231
  rtc.begin();
  rtc.setDateTime(__DATE__, __TIME__);
  rtc.setAlarm1(0, 3, 0, 0, DS3231_MATCH_H_M_S); //sets alarm for every 3 hours
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
  readSequence.reset();
  Serial.println("ph;temp;do;ec");
}

void loop() {
  readSequence.run();

  //sleeping, to be woken by interrupt pin
  attachInterrupt(digitalPinToInterrupt(intPin), wakeup, LOW);
  Serial.println("sleeping...");
  delay(100);
  sleep_cpu();

  //waking
  detachInterrupt(digitalPinToInterrupt(intPin));
  rtc.clearAlarm1();
  Serial.println("Awake!");
  RTCDateTime dt = rtc.getDateTime();
  Serial.println(rtc.dateFormat("H:i:s", dt));
  delay(100);

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

  strcpy(do_storage[transmitCounter], do_receive_buffer);
  strcpy(ph_storage[transmitCounter], ph_receive_buffer);
  strcpy(rtd_storage[transmitCounter], rtd_receive_buffer);
  strcpy(ec_storage[transmitCounter], ec_receive_buffer);
  
  if (transmitCounter == 3) {
    for (int i = 0; i < 4; i++) {
      Serial.print(ph_storage[i]);
      Serial.print(";");
      Serial.print(rtd_storage[i]);
      Serial.print(";");
      Serial.print(do_storage[i]);
      Serial.print(";");  
      Serial.print(ec_storage[i]);
      Serial.println();
    // This indicates EOF, and Arduino should sleep after this. (EOF = "\n"
    Serial.println();

    transmitCounter = 0;
    memset(do_storage, 0, sizeof(do_storage));
    memset(ph_storage, 0, sizeof(ph_storage));
    memset(rtd_storage, 0, sizeof(rtd_storage));
    memset(ec_storage, 0, sizeof(ec_storage));
    }
  } else {
    transmitCounter++;
  }

}

