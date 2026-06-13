// no-cats-land — firmware (faza 2: detekcja -> krotki puls pompki + cooldown)
// Arduino Nano 33 IoT (SAMD21).  PIR OUT -> D2,  MOSFET SIG -> D3.
// UWAGA: Nano 33 IoT = 3.3V, piny NIE sa 5V-tolerant. OUT z PIR to 3.3V (OK).
//
// Ulepszenia wzorowane na prior-art (asafdabush/POOPCAT) + lekcje z naszej sesji:
//  - START ROZBROJONY — pompka nie ruszy sama po wlozeniu wtyczki (woda gorzej wybacza).
//    Uzbrajanie/rozbrajanie komenda przez Serial: 'a' = arm, 'd' = disarm.
//  - MOTION_HOLD_MS — ruch musi sie UTRZYMAC, zanim strzelimy (tnie falszywki PIR-a).
//  - licznik strzalow + bezpiecznik MAX_SHOTS (auto-rozbrojenie przy zbytniej serii).
//  - log tylko przy zmianie stanu (bez zalewania Serial).
//  - D2 czytany jako INPUT_PULLDOWN (luzny OUT nie "plywa" — bug z naszej sesji).

// ---- Piny ----
const int PIR_PIN  = 2;             // OUT z PIR (HIGH = ruch)
const int PUMP_PIN = 3;             // SIG modulu MOSFET (HIGH = pompka ON)
const int LED_PIN  = LED_BUILTIN;

// ---- Czasy / parametry (ms) — wszystko do strojenia w jednym miejscu ----
const unsigned long WARMUP_MS      = 60000;  // rozgrzewka PIR (HC-SR501 ~60s)
const unsigned long MOTION_HOLD_MS = 600;    // ruch musi trwac tyle, zanim odpalimy
const unsigned long PULSE_MS       = 500;    // dlugosc strzalu wody — KROTKO (anty-zalanie)
const unsigned long COOLDOWN_MS    = 15000;  // przerwa po strzale (ignoruj ruch)
const int           MAX_SHOTS      = 20;     // bezpiecznik: po tylu strzalach auto-rozbrojenie

// ---- Stan ----
enum State { WARMUP, IDLE, ACTIVE, COOLDOWN };
State state = WARMUP;
bool  armed = false;                // START ROZBROJONY
unsigned long bootTime = 0, stateSince = 0, motionStart = 0;
int   shots = 0;

void setArmed(bool a) {
  if (a == armed) return;
  armed = a;
  digitalWrite(PUMP_PIN, LOW);
  motionStart = 0;
  stateSince = millis();
  if (armed) {
    shots = 0;
    state = (millis() - bootTime < WARMUP_MS) ? WARMUP : IDLE;
    Serial.println(">>> UZBROJONY (wpisz 'd' aby rozbroic)");
  } else {
    state = IDLE;
    Serial.println(">>> ROZBROJONY (wpisz 'a' aby uzbroic)");
  }
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'a' || c == 'A') setArmed(true);
    else if (c == 'd' || c == 'D') setArmed(false);
  }
}

void fire() {
  digitalWrite(PUMP_PIN, HIGH);
  state = ACTIVE;
  stateSince = millis();
  shots++;
  Serial.print(">>> STRZAL nr ");
  Serial.println(shots);
}

void setup() {
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  bootTime = stateSince = millis();
  Serial.println("no-cats-land start. START ROZBROJONY. Wpisz 'a' aby uzbroic.");
}

void loop() {
  unsigned long now = millis();
  handleSerial();

  // ROZBROJONY: nic nie strzela, dioda "oddycha" bardzo wolno
  if (!armed) {
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(LED_PIN, (now / 1000) % 2);
    return;
  }

  // ROZGRZEWKA PIR-a (po niej dopiero ufamy odczytom)
  if (state == WARMUP) {
    digitalWrite(LED_PIN, (now / 300) % 2);      // szybsze miganie = rozgrzewka
    if (now - bootTime >= WARMUP_MS) {
      state = IDLE; stateSince = now;
      Serial.println("rozgrzany, czuwam");
    }
    return;
  }

  bool motion = digitalRead(PIR_PIN) == HIGH;

  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN, motion ? HIGH : LOW); // podglad: dioda = biezacy ruch
      if (motion) {
        if (motionStart == 0) motionStart = now;
        if (now - motionStart >= MOTION_HOLD_MS) fire();   // ruch sie utrzymal -> strzal
      } else {
        motionStart = 0;                          // ruch znikl -> reset licznika debounce
      }
      break;

    case ACTIVE:                                  // pompka leci krotki puls
      digitalWrite(LED_PIN, HIGH);
      if (now - stateSince >= PULSE_MS) {
        digitalWrite(PUMP_PIN, LOW);
        state = COOLDOWN; stateSince = now;
        Serial.println("--- puls koniec, cooldown");
      }
      break;

    case COOLDOWN:                                // ignoruj ruch przez chwile
      digitalWrite(LED_PIN, LOW);
      if (now - stateSince >= COOLDOWN_MS) {
        if (shots >= MAX_SHOTS) {                 // za duzo strzalow -> bezpiecznik
          Serial.println(">>> limit strzalow osiagniety — rozbrajam dla bezpieczenstwa");
          setArmed(false);
        } else {
          state = IDLE; motionStart = 0;
          Serial.println("gotowy");
        }
      }
      break;

    case WARMUP:                                  // obsluzony wyzej
      break;
  }
}
