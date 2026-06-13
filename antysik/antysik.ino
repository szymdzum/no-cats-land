// antysik — odstraszacz kota na Arduino Nano 33 IoT
// Czujnik ruchu PIR (HC-SR501) wykrywa kota -> wlacza dmuchawe/silnik przez MOSFET.
// FQBN: arduino:samd:nano_33_iot
//
// UWAGA: Nano 33 IoT to plytka 3.3V — piny NIE sa 5V-tolerant.
// HC-SR501 wystawia na OUT logike 3.3V, wiec mozna go czytac bezpiecznie.

// ---- Piny ----
const int PIR_PIN = 2;   // wyjscie czujnika PIR (cyfrowe HIGH gdy ruch)
const int FAN_PIN = 3;   // sygnal do bramki MOSFET (PWM — regulacja sily)
const int LED_PIN = LED_BUILTIN;

// ---- Parametry zachowania (ms) ----
const unsigned long WARMUP_MS   = 30000;  // PIR stabilizuje sie po wlaczeniu (~30-60s)
const unsigned long ACTIVE_MS   = 4000;   // jak dlugo dmucha po wykryciu
const unsigned long COOLDOWN_MS = 8000;   // przerwa, zeby nie meczyc bez konca
const int FAN_POWER = 255;                // moc dmuchawy 0-255 (PWM)

// ---- Stan ----
enum State { IDLE, ACTIVE, COOLDOWN };
State state = IDLE;
unsigned long stateSince = 0;
unsigned long bootTime = 0;

void setup() {
  pinMode(PIR_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  analogWrite(FAN_PIN, 0);   // dmuchawa wylaczona na start
  digitalWrite(LED_PIN, LOW);

  Serial.begin(9600);
  bootTime = millis();
  stateSince = bootTime;
}

void startDeterrent() {
  state = ACTIVE;
  stateSince = millis();
  analogWrite(FAN_PIN, FAN_POWER);  // dmuchawa na full
  Serial.println(">>> KOT WYKRYTY — odstraszam!");
}

void stopDeterrent() {
  analogWrite(FAN_PIN, 0);
  state = COOLDOWN;
  stateSince = millis();
  Serial.println("--- koniec, cooldown");
}

void loop() {
  unsigned long now = millis();

  // PIR jeszcze sie nie ustabilizowal — czekamy, miga dioda powoli
  if (now - bootTime < WARMUP_MS) {
    digitalWrite(LED_PIN, (now / 500) % 2);  // wolne miganie = "rozgrzewam sie"
    return;
  }

  bool motion = digitalRead(PIR_PIN) == HIGH;

  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN, LOW);
      if (motion) startDeterrent();
      break;

    case ACTIVE:
      digitalWrite(LED_PIN, (now / 100) % 2);  // szybkie miganie = dziala
      if (now - stateSince >= ACTIVE_MS) stopDeterrent();
      break;

    case COOLDOWN:
      digitalWrite(LED_PIN, LOW);
      if (now - stateSince >= COOLDOWN_MS) {
        state = IDLE;
        Serial.println("gotowy");
      }
      break;
  }
}
