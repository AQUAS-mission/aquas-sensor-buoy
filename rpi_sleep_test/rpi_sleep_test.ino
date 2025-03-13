//Must include package -- search for "Arduino Low Power" and install
// #include "ArduinoLowPower.h"

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(7, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(5000);
  digitalWrite(7, HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
  digitalWrite(7, LOW);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100000);
  // LowPower.deepSleep(10000);

}
