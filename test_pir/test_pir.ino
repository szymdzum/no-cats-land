// test_pir — motion preview: the LED mirrors what the sensor sees.
//   motion detected -> LED ON
//   idle            -> LED OFF
// PULLDOWN holds the pin at 0 when the sensor is quiet (so it doesn't "float").

const int PIR_PIN = 2;
const int LED = LED_BUILTIN;

void setup() {
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(LED, OUTPUT);
}

void loop() {
  if (digitalRead(PIR_PIN) == HIGH) {
    digitalWrite(LED, HIGH);   // motion!
  } else {
    digitalWrite(LED, LOW);    // idle
  }
}
