#include <Ezo_i2c.h>
#include <Wire.h>
#include <sequencer3.h>
#include <sequencer4.h>
#include <Ezo_i2c_util.h>

Ezo_board DO = Ezo_board(97, "DO");
Ezo_board PH = Ezo_board(99, "PH");
Ezo_board EC = Ezo_board(100, "EC");
Ezo_board RTD = Ezo_board(102, "RTD");

void step1();
void step2();
void step3();

Sequencer3 readSequence(&step1, 1000, &step2, 1000, &step3, 1000);

void setup() {
  Wire.begin();
  Serial.begin(9600);
  readSequence.reset();
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
  Serial.print("PH reading ");
  receive_and_print_reading(PH);
  Serial.println();
  Serial.print("RTD Temp ");
  receive_and_print_reading(RTD);
  Serial.println();
  Serial.print("DO reading ");
  receive_and_print_reading(DO);
  Serial.println();

  if ((RTD.get_error() == Ezo_board::SUCCESS) && (RTD.get_last_received_reading() > -1000.0)) {
    EC.send_read_with_temp_comp(RTD.get_last_received_reading());
  } else {
    // Default case: EC with 25˚C default temperature
    EC.send_read_with_temp_comp(25.0);
  }
}

void step3(){
  Serial.print("EC reading ");
  receive_and_print_reading(EC);
  Serial.println();
}

