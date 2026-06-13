// Setup smoke-test for Arduino Nano 33 IoT.
// Blinks the built-in L LED (LED_BUILTIN) at 200ms ON / 800ms OFF.
// A rhythm different from the factory Blink = instant proof the new sketch uploaded.

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(800);
}
