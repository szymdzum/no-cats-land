// test_pir — pokaz ruchu: dioda odbija to, co widzi czujnik.
//   RUCH wykryty  -> dioda SWIECI
//   spokoj        -> dioda ZGASZONA
// PULLDOWN trzyma pin na 0, gdy czujnik milczy (zeby nie "plywal").

const int PIR_PIN = 2;
const int LED = LED_BUILTIN;

void setup() {
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(LED, OUTPUT);
}

void loop() {
  if (digitalRead(PIR_PIN) == HIGH) {
    digitalWrite(LED, HIGH);   // ruch!
  } else {
    digitalWrite(LED, LOW);    // spokoj
  }
}
