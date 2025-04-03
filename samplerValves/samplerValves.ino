int CONTAINER_1_PIN = 7;
int CONTAINER_2_PIN = 8;
int CONTAINER_3_PIN = 9;
int SENSOR_1_PIN = 10;
int SENSOR_2_PIN = 11;
int SENSOR_3_PIN = 12;

struct containerStatus {
  int container_pin;
  int sensor_pin;
  bool is_filled;
  bool is_open;
};

containerStatus[3] c;

void setup() {
  // put your setup code here, to run once:
  c[0].container_pin = 7;
  c[1].container_pin = 8;
  c[2].container_pin = 9;
  c[0].sensor_pin = 10;
  c[1].sensor_pin = 11;
  c[2].sensor_pin = 12;
  pinMode(c[0].container_pin, OUTPUT);
  pinMode(c[1].container_pin, OUTPUT);
  pinMode(c[2].container_pin, OUTPUT);
  pinMode(c[0].sensor_pin, INPUT);
  pinMode(c[1].sensor_pin, INPUT);
  pinMode(c[2].sensor_pin, INPUT);
}

//1 <= ContainerNr <= 3
void fillContainer(int containerNr) {
  
}



//Set the speed of the pump between 
void setPumpSpeed(int value) {
  if (value >= 0 && value <= 255) {
    analogWrite(value);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  setPumpSpeed(0);
  delay(1000);
  setPumpSpeed(200);
}
