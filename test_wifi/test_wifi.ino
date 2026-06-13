// test_wifi — Nano joins WiFi (DHCP address) and serves the sensor state.
//   Open http://<board-ip>/  -> "MOTION" or "idle"
//   LED: blinks while connecting, then shows the sensor state (on = motion).
//   The IP is printed over serial.

#include <WiFiNINA.h>
#include "arduino_secrets.h"

const int PIR_PIN = 2;
const int LED = LED_BUILTIN;
WiFiServer server(80);

void setup() {
  pinMode(PIR_PIN, INPUT_PULLDOWN);
  pinMode(LED, OUTPUT);
  Serial.begin(9600);

  WiFi.begin(SECRET_SSID, SECRET_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED, HIGH); delay(100);
    digitalWrite(LED, LOW);  delay(100);
  }
  server.begin();
  Serial.print("IP="); Serial.println(WiFi.localIP());
}

void loop() {
  int motion = digitalRead(PIR_PIN);
  digitalWrite(LED, motion);

  WiFiClient client = server.available();
  if (client) {
    client.readStringUntil('\n');         // request line
    while (client.available()) client.read();  // ignore the rest
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println(motion == HIGH ? "MOTION" : "idle");
    client.print("raw=");
    client.println(motion);
    delay(2);
    client.stop();
  }

  static unsigned long last = 0;
  if (millis() - last > 3000) { last = millis(); Serial.print("IP="); Serial.println(WiFi.localIP()); }
}
