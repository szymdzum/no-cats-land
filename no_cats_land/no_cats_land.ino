// no-cats-land — firmware (phase 2): local detection + 12V pump pulse, controllable
// from Home Assistant over WiFi (REST). Detection and firing are LOCAL and keep working
// even if WiFi/HA is down; HA only arms/disarms and monitors.
// Arduino Nano 33 IoT (SAMD21). PIR OUT -> D2, MOSFET SIG -> D3.
//
// HTTP endpoints (one request per connection):
//   GET /status  -> JSON {"armed":bool,"state":"...","motion":0|1,"shots":n}
//   GET /arm     -> arm,    then returns status JSON
//   GET /disarm  -> disarm, then returns status JSON
// Arming also works over Serial ('a' / 'd').
//
// Safety/robustness design choices:
//  - starts DISARMED (pump never fires on power-up)
//  - MOTION_HOLD_MS debounce (fire only after motion persists)
//  - short PULSE_MS + COOLDOWN_MS (anti-flood)
//  - MAX_SHOTS auto-disarm (runaway guard)
//  - D2 read as INPUT_PULLDOWN (a loose OUT wire won't float)
//  - ~60s PIR warm-up before trusting readings

#include <WiFiNINA.h>
#include "arduino_secrets.h"

// ---- Pins ----
const int PIR_PIN  = 2;
const int PUMP_PIN = 3;
const int LED_PIN  = LED_BUILTIN;

// ---- Timing / parameters (ms) ----
const unsigned long WARMUP_MS      = 60000;
const unsigned long MOTION_HOLD_MS = 600;
const unsigned long PULSE_MS       = 500;
const unsigned long COOLDOWN_MS    = 15000;
const int           MAX_SHOTS      = 20;
const unsigned long WIFI_RETRY_MS  = 30000;

// ---- State ----
enum State { WARMUP, IDLE, ACTIVE, COOLDOWN };
State state = WARMUP;
bool  armed = false;                // starts DISARMED
unsigned long bootTime = 0, stateSince = 0, motionStart = 0, lastWifiTry = 0;
int   shots = 0;

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
  digitalWrite(PUMP_PIN, LOW);
  motionStart = 0;
  stateSince = millis();
  if (armed) {
    shots = 0;
    state = (millis() - bootTime < WARMUP_MS) ? WARMUP : IDLE;
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
  shots++;
  Serial.print(">>> SHOT #");
  Serial.println(shots);
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'a' || c == 'A') setArmed(true);
    else if (c == 'd' || c == 'D') setArmed(false);
  }
}

// Bounded WiFi connect — never spins forever, so detection always runs.
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

void sendStatus(WiFiClient& c) {
  int motion = (state != WARMUP && digitalRead(PIR_PIN) == HIGH) ? 1 : 0;
  c.println("HTTP/1.1 200 OK");
  c.println("Content-Type: application/json");
  c.println("Connection: close");
  c.println();
  c.print("{\"armed\":");   c.print(armed ? "true" : "false");
  c.print(",\"state\":\""); c.print(stateName());
  c.print("\",\"motion\":"); c.print(motion);
  c.print(",\"shots\":");   c.print(shots);
  c.println("}");
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;
  String req = client.readStringUntil('\n');     // e.g. "GET /arm HTTP/1.1"
  while (client.available()) client.read();        // drain the rest
  if (req.indexOf("/disarm") >= 0)      setArmed(false);
  else if (req.indexOf("/arm") >= 0)    setArmed(true);
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
  Serial.println("no-cats-land start. DISARMED.");
  wifiConnect();
}

void loop() {
  unsigned long now = millis();
  handleSerial();

  // WiFi upkeep — never while ACTIVE, so a reconnect stall can't extend a water pulse
  if (state != ACTIVE && WiFi.status() != WL_CONNECTED && now - lastWifiTry > WIFI_RETRY_MS) {
    wifiConnect();
    now = millis();
  }
  // Serve HTTP only when not firing (readStringUntil could stall up to ~1s)
  if (state != ACTIVE && serverUp && WiFi.status() == WL_CONNECTED) handleClient();

  // DISARMED: nothing fires, LED breathes slowly
  if (!armed) {
    digitalWrite(PUMP_PIN, LOW);
    digitalWrite(LED_PIN, (now / 1000) % 2);
    return;
  }

  // PIR warm-up
  if (state == WARMUP) {
    digitalWrite(LED_PIN, (now / 300) % 2);
    if (now - bootTime >= WARMUP_MS) { state = IDLE; stateSince = now; Serial.println("warmed up, watching"); }
    return;
  }

  bool motion = digitalRead(PIR_PIN) == HIGH;

  switch (state) {
    case IDLE:
      digitalWrite(LED_PIN, motion ? HIGH : LOW);
      if (motion) {
        if (motionStart == 0) motionStart = now;
        if (now - motionStart >= MOTION_HOLD_MS) fire();
      } else {
        motionStart = 0;
      }
      break;

    case ACTIVE:
      digitalWrite(LED_PIN, HIGH);
      if (now - stateSince >= PULSE_MS) {
        digitalWrite(PUMP_PIN, LOW);
        state = COOLDOWN; stateSince = now;
        Serial.println("--- pulse done, cooldown");
      }
      break;

    case COOLDOWN:
      digitalWrite(LED_PIN, LOW);
      if (now - stateSince >= COOLDOWN_MS) {
        if (shots >= MAX_SHOTS) {
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
