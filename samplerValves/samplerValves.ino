// Pin definitions
const int CONTAINER_PINS[3] = {7, 8, 9};
const int SENSOR_PINS[3] = {10, 11, 12};
const int PUMP_PIN = 5;               // PWM pin for pump speed
const int OUTFLOW_SOLENOID_PIN = 6;   // Pin to control outflow solenoid
const int SAMPLE_TRIGGER_PIN = 2;     // Digital pin used to trigger sampling

// Container status struct
struct ContainerStatus {
  int container_pin;
  int sensor_pin;
  bool is_filled;
};

ContainerStatus containers[3];
int currentSampleContainer = 0;  // Tracks which container to fill

void setup() {
  Serial.begin(9600);
  
  // Initialize container and sensor pins
  for (int i = 0; i < 3; i++) {
    containers[i].container_pin = CONTAINER_PINS[i];
    containers[i].sensor_pin = SENSOR_PINS[i];
    containers[i].is_filled = false;
    pinMode(containers[i].container_pin, OUTPUT);
    pinMode(containers[i].sensor_pin, INPUT);
  }

  // Initialize solenoid and pump pins
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(OUTFLOW_SOLENOID_PIN, OUTPUT);
  pinMode(SAMPLE_TRIGGER_PIN, INPUT);

  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(OUTFLOW_SOLENOID_PIN, LOW);
}

// Sets pump speed via PWM (0-255)
void setPumpSpeed(int value) {
  value = constrain(value, 0, 255);
  analogWrite(PUMP_PIN, value);
}

// Purge system: flush lines for specified duration (ms)
void purge(unsigned long duration) {
  // Close all sample solenoids
  for (int i = 0; i < 3; i++) {
    digitalWrite(containers[i].container_pin, LOW);
  }

  // Open outflow solenoid
  digitalWrite(OUTFLOW_SOLENOID_PIN, HIGH);
  setPumpSpeed(200);

  delay(duration);

  // Stop pump and close outflow
  setPumpSpeed(0);
  digitalWrite(OUTFLOW_SOLENOID_PIN, LOW);
}


// GPT4 logic tested below
// Sample into the current container (guardrails included)
void sample() {
  if (currentSampleContainer >= 3) {
    Serial.println("All containers filled.");
    return;
  }

  ContainerStatus &csc = containers[currentSampleContainer];

  // Check if already filled
  if (digitalRead(csc.sensor_pin) == HIGH) {
    Serial.print("Container "); Serial.print(currentSampleContainer + 1);
    Serial.println(" is already filled.");
    csc.is_filled = true;
    currentSampleContainer++;
    return;
  }

  // Begin sampling
  digitalWrite(csc.container_pin, HIGH);  // Open solenoid
  setPumpSpeed(200);

  unsigned long startTime = millis();
  unsigned long timeout = 10000;  // 10 seconds safety timeout

  // Wait until the container is filled or timeout
  while (digitalRead(csc.sensor_pin) == LOW) {
    if (millis() - startTime > timeout) {
      Serial.println("Sampling timeout: sensor did not trigger.");
      break;
    }
  }

  // Stop pump and close solenoid
  setPumpSpeed(0);
  digitalWrite(csc.container_pin, LOW);

  // Mark container as filled and move to next
  csc.is_filled = true;
  currentSampleContainer++;

  Serial.print("Sample collected in container ");
  Serial.println(currentSampleContainer);
}

void loop() {
  // Sample when digital pin goes HIGH (rising edge logic could be added)
  if (digitalRead(SAMPLE_TRIGGER_PIN) == HIGH) {
    sample();
    delay(1000); // Debounce delay
  }

} 
