// no-cats-land — firmware (phase 2: detect motion -> short pump pulse + cooldown)
// Arduino Nano 33 IoT (SAMD21).  PIR OUT -> D2,  MOSFET SIG -> D3.
// NOTE: Nano 33 IoT is 3.3V, pins are NOT 5V tolerant. PIR OUT is 3.3V (safe).
//
// Safety/robustness design choices:
//  - Starts DISARMED — the pump won't fire on its own when power is applied
//    (water is less forgiving). Arm/disarm over Serial: 'a' = arm, 'd' = disarm.
//  - MOTION_HOLD_MS — motion must persist before we fire (rejects PIR glitches).
//  - shot counter + MAX_SHOTS safety (auto-disarm on a runaway burst).
//  - logs only on state changes (no Serial spam).
//  - D2 read as INPUT_PULLDOWN (a loose OUT wire won't "float" — session bug).

// ---- Pins ----
const int PIR_PIN  = 2;             // PIR OUT (HIGH = motion)
const int PUMP_PIN = 3;             // MOSFET module SIG (HIGH = pump ON)
const int LED_PIN  = LED_BUILTIN;

// ---- Timing / parameters (ms) — all tunables in one place ----
const unsigned long WARMUP_MS      = 60000;  // PIR warm-up (HC-SR501 ~60s)
const unsigned long MOTION_HOLD_MS = 600;    // motion must last this long before firing
const unsigned long PULSE_MS       = 500;    // water shot length — SHORT (anti-flood)
const unsigned long COOLDOWN_MS    = 15000;  // pause after a shot (ignore motion)
const int           MAX_SHOTS      = 20;     // safety: auto-disarm after this many shots

// ---- State ----
enum State { WARMUP, IDLE, ACTIVE, COOLDOWN };
State state = WARMUP;
bool  armed = false;                // starts DISARMED
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
    Serial.println(">>> ARMED (send 'd' to disarm)");
  } else {
    state = IDLE;
    Serial.println(">>> DISARMED (send 'a' to arm)");
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
  Serial.print(">>> SHOT #");
  Serial.println(shots);
}

void setup() {
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  bootTime = stateSince = millis();
  Serial.println("no-cats-land start. DISARMED. Send 'a' to arm.");
}

void loop() {
  unsigned long now = millis();
  handleSerial();

  // DISARMED: nothing fires, LED "breathes" very slowly
  if (!armed) {
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(LED_PIN, (now / 1000) % 2);
    return;
  }

  // PIR warm-up (only trust readings afterwards)
  if (state == WARMUP) {
    digitalWrite(LED_PIN, (now / 300) % 2);      // faster blink = warming up
    if (now - bootTime >= WARMUP_MS) {
      state = IDLE; stateSince = now;
      Serial.println("warmed up, watching");
    }
    return;
  }

  bool motion = digitalRead(PIR_PIN) == HIGH;

  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN, motion ? HIGH : LOW); // preview: LED = current motion
      if (motion) {
        if (motionStart == 0) motionStart = now;
        if (now - motionStart >= MOTION_HOLD_MS) fire();   // motion persisted -> fire
      } else {
        motionStart = 0;                          // motion gone -> reset debounce
      }
      break;

    case ACTIVE:                                  // pump runs a short pulse
      digitalWrite(LED_PIN, HIGH);
      if (now - stateSince >= PULSE_MS) {
        digitalWrite(PUMP_PIN, LOW);
        state = COOLDOWN; stateSince = now;
        Serial.println("--- pulse done, cooldown");
      }
      break;

    case COOLDOWN:                                // ignore motion for a while
      digitalWrite(LED_PIN, LOW);
      if (now - stateSince >= COOLDOWN_MS) {
        if (shots >= MAX_SHOTS) {                 // too many shots -> safety
          Serial.println(">>> shot limit reached — disarming for safety");
          setArmed(false);
        } else {
          state = IDLE; motionStart = 0;
          Serial.println("ready");
        }
      }
      break;

    case WARMUP:                                  // handled above
      break;
  }
}
