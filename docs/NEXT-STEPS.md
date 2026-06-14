# no-cats-land — state of play & next steps

A "resume here" checklist for when the rest of the hardware arrives. Full hardware context
(BOM, power architecture, wiring diagram, gotchas) lives in [`../CLAUDE.md`](../CLAUDE.md).

## ✅ Done (works today)

- Firmware (`no_cats_land/`): local state machine warm-up → idle → pulse → cooldown,
  motion debounce, tunable timings + shot cap, **armed state + shot count persisted to flash**.
- WiFi + REST API (`docs/api.md`): `/status` telemetry, `/arm` `/disarm` `/test` `/reset` `/set`.
- Home Assistant: package (`docs/home-assistant.yaml`) + Mushroom dashboard
  (`docs/dashboard-no-cats-land.yaml`) — arm switch, telemetry, tuning sliders,
  shot + auto-disarm notifications.
- PIR roughly calibrated (left pot = sensitivity, right pot = hold time).
- Board has a **DHCP reservation → 192.168.1.23** (MAC `78:21:84:ac:12:60`).

## 🛒 To buy (Botland unless noted)

Core (makes it actually squirt water — ~130 zł):
- [12V diaphragm pump (110 l/h, 7mm)](https://botland.com.pl/pompy/7206-pompa-do-cieczy-12v-110lh-7mm-5904422335595.html) — 26,90
- [MOSFET driver Adafruit 5648 (works at 3.3V)](https://botland.com.pl/sterowniki-silnikow-moduly/22539-sterownik-mosfet-z-kanalem-n-do-silnikow-elektromagnesow-diod-led-stemma-jst-ph-2mm-adafruit-5648.html) — 23,90
  - cheaper alt (AliExpress): **AOD4184 / D4184 opto-isolated MOSFET module** (~5–10 zł) — NOT IRF520
- [Flyback diode 1N4007 (10pcs)](https://botland.com.pl/diody-prostownicze/1803-dioda-prostownicza-1n4007-1a-1000v-10szt-5903351244435.html) — 1,00
- [12V/3A PSU, 5.5/2.5mm plug](https://botland.com.pl/zasilacze-dogniazdkowe/8851-zasilacz-impulsowy-12v3a-wtyk-dc-5525mm-5904422312299.html) — 34,90
- [DC jack 2.5/5.5 for breadboard](https://botland.com.pl/akcesoria-do-plytek-stykowych/1610-gniazdo-dc-2555mm-do-prototypowej-plytki-stykowej-5903351245999.html) — 8,90 (must be **2.5mm** to match the PSU)
- **Silicone/aquarium tube ~7mm** — NOT Botland: pet shop / hardware store / AliExpress (a few zł)
- Reservoir = a bottle

Workshop / nice-to-have (optional):
- [Breadboard 830 + power module + wires](https://botland.com.pl/plytki-stykowe/1670-zestaw-plytka-stykowa-830-przewody-modul-zasilajacy-5903351241472.html) — 25,90
- [Jumper wire set, all genders (120pcs)](https://botland.com.pl/przewody-polaczeniowe/19946-zestaw-przewodow-polaczeniowych-justpi-20cm-3x40szt-m-m-z-z-m-z-120szt-5904422328702.html) — 17,90
- [Components kit Iduino KTS042](https://botland.com.pl/zestawy-startowe-dla-arduino/14319-zestaw-elementow-elektronicznych-dla-arduino-iduino-kts042-5903351241687.html) — 39,90 (resistors/LEDs/caps/buttons/**buzzer** = optional 2nd deterrent)
- A multimeter (buy-once); cheapest on Botland ~86 zł, or a DT-830 ~20 zł elsewhere
- For future cheap WiFi projects: **ESP32** (~25–40 zł AliExpress) + ESPHome (less code, native HA)

## 🔌 When hardware arrives — wiring

Follow the diagram in [`../CLAUDE.md`](../CLAUDE.md) ("Wiring diagram"). In short:
- Single **12V** rail feeds: PIR VCC, Nano **VIN**, and the pump (via MOSFET). **Common ground** everywhere.
- Pump switched by the MOSFET module; **MOSFET signal pin → Nano D3** (`PUMP_PIN`).
- **Flyback diode across the pump** (cathode/stripe to +12V) — mandatory, the pump is inductive.
- PIR `OUT → D2` (already wired); on 12V the PIR runs off VCC, OUT stays 3.3V (safe).
- ⚠️ Do not apply 12V until PIR VCC/GND are confirmed. Nano pins are **3.3V, not 5V-tolerant**.

## 🧪 Test order (fewest things that can break first)

1. Pump on a separate 12V, **dry, no water**, flyback diode in place. Use `/test` (or HA "Test fire").
2. Add water + tube + reservoir. Tune `pulse` / `cooldown` with the HA sliders.
3. Set a sane `max_shots` (e.g. 20) and `warmup` (60s) for deployment.
4. Mount in the alcove last.

## 🐛 Known gremlins (and the real fix)

Today's flakiness on the breadboard was all from two things — both disappear on the final build:
- **Random reboots / "disarms itself"** → unstable USB power (dock/loose cable) = brownouts.
  Fix: run off the **stable 12V PSU**, not USB through the dock.
- **PIR shows motion=0 / intermittent** → a loose OUT dupont on the breadboard.
  Fix: firm / soldered connections at final mounting.

## 🧭 Quick reference

- Board: Arduino Nano 33 IoT · FQBN `arduino:samd:nano_33_iot` · core `arduino:samd`
- IP `192.168.1.23` (DHCP-reserved) · USB serial port `/dev/cu.usbmodem*`
- Build/upload:
  ```bash
  arduino-cli compile --fqbn arduino:samd:nano_33_iot ./no_cats_land
  arduino-cli upload -p <port> --fqbn arduino:samd:nano_33_iot ./no_cats_land
  ```
- WiFi creds: copy `arduino_secrets.example.h` → `arduino_secrets.h` (gitignored).
- Serial read on this board: `arduino-cli monitor` returns 0 bytes; use
  `stty -f <port> 9600 raw -echo; cat <port>` instead, or read state over WiFi.

## 🔮 Future — phase 3: "only a cat"

PIR fires on any warm motion (incl. human legs). To fire **only for a cat**:
- A WiFi RTSP camera (e.g. TP-Link Tapo) → **Frigate** on the Pi 5 (object detection knows `cat`)
  → HA automation calls `/arm` + `/test` only when a cat is in the zone. Pi 5 (16GB, idle) can
  run Frigate for one camera on CPU; a Coral TPU is optional.
