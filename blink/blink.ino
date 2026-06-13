// Test setup Arduino Nano 33 IoT
// Miganie wbudowanej diody L (pin LED_BUILTIN) w rytmie 200ms ON / 800ms OFF
// Inny rytm niz fabryczny Blink = od razu widac, ze nowy szkic sie wgral.

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
  delay(800);
}
