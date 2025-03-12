#include <Ezo_i2c.h>
#include <Wire.h>
#include <sequencer3.h>
#include <sequencer4.h>
#include <Ezo_i2c_util.h>

Ezo_board DO = Ezo_board(97, "DO"); //dissolved oxygen
Ezo_board PH = Ezo_board(99, "PH"); //ph
Ezo_board EC = Ezo_board(100, "EC"); //electrical conductivity
Ezo_board RTD = Ezo_board(102, "RTD"); //temperature

char ph_receive_buffer[32]; 
char rtd_receive_buffer[32]; 
char do_receive_buffer[32]; 
char ec_receive_buffer[32];
// prints <ph>;<temp>;<do>;<ec>


void step1();
void step2();
void step3();

Sequencer3 readSequence(&step1, 1000, &step2, 1000, &step3, 1000);

void setup() {
  Wire.begin();
  Serial.begin(9600);
  readSequence.reset();
  Serial.println("ph;temp;do;ec");
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
  Serial.print(ph_receive_buffer);
  Serial.print(";");
  Serial.print(rtd_receive_buffer);
  Serial.print(";");
  Serial.print(do_receive_buffer);
  Serial.print(";");
  EC.receive_cmd(ec_receive_buffer,32);
  Serial.print(ec_receive_buffer);
  Serial.println();
}

