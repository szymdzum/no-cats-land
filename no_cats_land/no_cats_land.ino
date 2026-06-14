// no-cats-land — firmware (phase 2): local detection + 12V pump pulse, full Home Assistant
// integration over WiFi (REST). Detection, telemetry and firing are LOCAL and keep working
// even if WiFi/HA is down; HA arms/disarms, monitors and tunes.
// Arduino Nano 33 IoT (SAMD21). PIR OUT -> D2, MOSFET SIG -> D3.
//
// HTTP endpoints (one request per connection); all return the status JSON:
//   GET /status                          -> telemetry (see sendStatus)
//   GET /arm        GET /disarm          -> arm / disarm
//   GET /test                            -> fire ONE manual test pulse (works even disarmed)
//   GET /reset                           -> reset the telemetry counters
//   GET /set?pulse=&cooldown=&hold=&warmup=  -> live-tune timings (ms), no reflash
// Arming also works over Serial ('a' / 'd').
//
// Design choices: starts DISARMED; motion sensing + telemetry run even when disarmed
// (so you can calibrate from HA) but the pump only fires when ARMED; MOTION_HOLD debounce;
// short pulse + cooldown (anti-flood); MAX_SHOTS auto-disarm; INPUT_PULLDOWN on D2.

#include <WiFiNINA.h>
#include <FlashStorage.h>
#include "arduino_secrets.h"

// Persisted armed state — survives reboots / power blips (SAMD has no real EEPROM).
// magic byte guards against reading uninitialized flash (0xFF) as "armed".
typedef struct { uint8_t magic; bool armed; } Persist;
const uint8_t PERSIST_MAGIC = 0xA5;
FlashStorage(armedFlash, Persist);

// ---- Pins ----
const int PIR_PIN  = 2;
const int PUMP_PIN = 3;
const int LED_PIN  = LED_BUILTIN;

// ---- Tunable timings (ms) — changeable at runtime via /set ----
unsigned long warmupMs     = 60000;
unsigned long motionHoldMs = 600;
unsigned long pulseMs      = 500;
unsigned long cooldownMs   = 15000;

// ---- Fixed safety cap ----
const int MAX_SHOTS = 20;            // auto-disarm after this many shots per arming
const unsigned long WIFI_RETRY_MS = 30000;

// ---- State ----
enum State { WARMUP, IDLE, ACTIVE, COOLDOWN };
State state = WARMUP;
bool  armed = false;
unsigned long bootTime = 0, stateSince = 0, motionStart = 0, lastWifiTry = 0;
unsigned long lastMotionMs = 0, lastShotMs = 0, testPulseUntil = 0;
bool  prevMotion = false;
long  shotsTotal = 0, motionEvents = 0;
int   shotsSinceArm = 0;

WiFiServer server(80);
bool serverUp = false;

const char* stateName() {
  switch (state) {
    case WARMUP:   return "warmup";
    case IDLE:     return "idle";
    case ACTIVE:   return "active";
    case COOLDOWN: return "cooldown";
  }
  return "?";
}

void setArmed(bool a) {
  if (a == armed) return;
  armed = a;
  { Persist p; p.magic = PERSIST_MAGIC; p.armed = armed; armedFlash.write(p); }  // remember across reboots
  digitalWrite(PUMP_PIN, LOW);
  motionStart = 0;
  stateSince = millis();
  if (armed) {
    shotsSinceArm = 0;
    state = (millis() - bootTime < warmupMs) ? WARMUP : IDLE;
    Serial.println(">>> ARMED");
  } else {
    state = IDLE;
    Serial.println(">>> DISARMED");
  }
}

void fire() {
  digitalWrite(PUMP_PIN, HIGH);
  state = ACTIVE;
  stateSince = millis();
  lastShotMs = stateSince;
  shotsTotal++; shotsSinceArm++;
  Serial.print(">>> SHOT #"); Serial.println(shotsSinceArm);
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'a' || c == 'A') setArmed(true);
    else if (c == 'd' || c == 'D') setArmed(false);
  }
}

void wifiConnect() {
  lastWifiTry = millis();
  WiFi.begin(SECRET_SSID, SECRET_PASS);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 8000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    if (!serverUp) { server.begin(); serverUp = true; }
    Serial.print("WiFi IP="); Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi not connected (will retry); running locally");
  }
}

// extract an integer query param, e.g. paramInt(req, "pulse=", pulseMs)
long paramInt(const String& req, const char* key, long fallback) {
  int i = req.indexOf(key);
  if (i < 0) return fallback;
  return req.substring(i + strlen(key)).toInt();
}
long clampL(long v, long lo, long hi) { return v < lo ? lo : (v > hi ? hi : v); }

void sendStatus(WiFiClient& c) {
  unsigned long now = millis();
  int motion = (now - bootTime >= warmupMs && digitalRead(PIR_PIN) == HIGH) ? 1 : 0;
  long lastMotionS = lastMotionMs ? (long)((now - lastMotionMs) / 1000) : -1;
  long lastShotS   = lastShotMs   ? (long)((now - lastShotMs)   / 1000) : -1;
  long cooldownLeft = (state == COOLDOWN) ? clampL((long)((cooldownMs - (now - stateSince)) / 1000), 0, 100000) : 0;
  long rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;

  c.println("HTTP/1.1 200 OK");
  c.println("Content-Type: application/json");
  c.println("Connection: close");
  c.println();
  c.print("{\"armed\":");          c.print(armed ? "true" : "false");
  c.print(",\"state\":\"");        c.print(stateName());
  c.print("\",\"motion\":");       c.print(motion);
  c.print(",\"shots_total\":");    c.print(shotsTotal);
  c.print(",\"motion_events\":");  c.print(motionEvents);
  c.print(",\"last_motion_s\":");  c.print(lastMotionS);
  c.print(",\"last_shot_s\":");    c.print(lastShotS);
  c.print(",\"cooldown_left_s\":");c.print(cooldownLeft);
  c.print(",\"uptime_s\":");       c.print(now / 1000);
  c.print(",\"rssi\":");           c.print(rssi);
  c.print(",\"pulse_ms\":");       c.print(pulseMs);
  c.print(",\"cooldown_ms\":");    c.print(cooldownMs);
  c.print(",\"hold_ms\":");        c.print(motionHoldMs);
  c.print(",\"warmup_ms\":");      c.print(warmupMs);
  c.println("}");
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;
  String req = client.readStringUntil('\n');     // "GET /path?query HTTP/1.1"
  while (client.available()) client.read();

  if (req.indexOf("/disarm") >= 0)      setArmed(false);
  else if (req.indexOf("/arm") >= 0)    setArmed(true);
  else if (req.indexOf("/test") >= 0) {           // manual one-shot, even when disarmed
    digitalWrite(PUMP_PIN, HIGH);
    testPulseUntil = millis() + pulseMs;
    lastShotMs = millis();
    Serial.println(">>> TEST pulse");
  }
  else if (req.indexOf("/reset") >= 0) {          // reset telemetry counters
    shotsTotal = 0; motionEvents = 0; shotsSinceArm = 0;
    Serial.println(">>> counters reset");
  }
  else if (req.indexOf("/set") >= 0) {            // live-tune timings (ms)
    pulseMs      = clampL(paramInt(req, "pulse=",    pulseMs),      50, 5000);
    cooldownMs   = clampL(paramInt(req, "cooldown=", cooldownMs),   0,  600000);
    motionHoldMs = clampL(paramInt(req, "hold=",     motionHoldMs), 0,  10000);
    warmupMs     = clampL(paramInt(req, "warmup=",   warmupMs),     0,  120000);
    Serial.print(">>> set pulse="); Serial.print(pulseMs);
    Serial.print(" cooldown="); Serial.print(cooldownMs);
    Serial.print(" hold="); Serial.print(motionHoldMs);
    Serial.print(" warmup="); Serial.println(warmupMs);
  }

  sendStatus(client);
  delay(2);
  client.stop();
}

void setup() {
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  bootTime = stateSince = millis();

  // restore armed state from flash (survives reboots / power blips)
  Persist p = armedFlash.read();
  if (p.magic == PERSIST_MAGIC) {
    armed = p.armed;
    if (armed) state = WARMUP;          // re-arm, but still warm the PIR up first
  } else {
    Persist init; init.magic = PERSIST_MAGIC; init.armed = false;
    armedFlash.write(init);             // first boot after flashing -> default disarmed
  }
  Serial.print("no-cats-land start. ");
  Serial.println(armed ? "ARMED (restored from flash)" : "DISARMED");

  wifiConnect();
}

void loop() {
  unsigned long now = millis();
  handleSerial();

  // --- manual test pulse: overrides everything, brief, independent of armed ---
  if (testPulseUntil) {
    digitalWrite(PUMP_PIN, HIGH);
    digitalWrite(LED_PIN, (now / 80) % 2);        // fast flicker = test firing
    if (now >= testPulseUntil) {
      digitalWrite(PUMP_PIN, LOW);
      testPulseUntil = 0;
      Serial.println("--- test pulse done");
    }
    return;
  }

  // --- WiFi upkeep + HTTP (never while firing, so it can't extend a pulse) ---
  if (state != ACTIVE && WiFi.status() != WL_CONNECTED && now - lastWifiTry > WIFI_RETRY_MS) {
    wifiConnect();
    now = millis();
  }
  if (state != ACTIVE && serverUp && WiFi.status() == WL_CONNECTED) handleClient();

  // --- motion sensing + telemetry: runs even when DISARMED (for calibration in HA) ---
  bool warm = (now - bootTime >= warmupMs);
  bool motion = warm && (digitalRead(PIR_PIN) == HIGH);
  if (motion && !prevMotion) { motionEvents++; lastMotionMs = now; }
  prevMotion = motion;

  // --- DISARMED: never fire; LED breathes slowly ---
  if (!armed) {
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(LED_PIN, (now / 1000) % 2);
    if (state != IDLE) state = IDLE;
    return;
  }

  // --- ARMED ---
  if (!warm) { state = WARMUP; digitalWrite(LED_PIN, (now / 300) % 2); return; }
  if (state == WARMUP) { state = IDLE; stateSince = now; Serial.println("warmed up, watching"); }

  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN, motion ? HIGH : LOW);
      if (motion) {
        if (motionStart == 0) motionStart = now;
        if (now - motionStart >= motionHoldMs) fire();
      } else {
        motionStart = 0;
      }
      break;

    case ACTIVE:
      digitalWrite(LED_PIN, HIGH);
      if (now - stateSince >= pulseMs) {
        digitalWrite(PUMP_PIN, LOW);
        state = COOLDOWN; stateSince = now;
        Serial.println("--- pulse done, cooldown");
      }
      break;

    case COOLDOWN:
      digitalWrite(LED_PIN, LOW);
      if (now - stateSince >= cooldownMs) {
        if (shotsSinceArm >= MAX_SHOTS) {
          Serial.println(">>> shot limit reached — disarming for safety");
          setArmed(false);
        } else {
          state = IDLE; motionStart = 0;
          Serial.println("ready");
        }
      }
      break;

    case WARMUP:
      break;
  }
}
