# no-cats-land 🐱🚱

> *"The best-fortified line of defense is useless if the enemy simply walks around it."*
> — the spirit of the Maginot Line, patron of this project

A water- (and otherwise-) based, motion-triggered cat deterrent driven by an
**Arduino Nano 33 IoT**. A PIR sensor detects the cat entering an alcove → the
microcontroller fires a short, harmless burst from a 12V diaphragm pump.

The name plays on *no man's land* — and like the Maginot Line, it's a defense you can
simply walk around.

## Hardware (summary — full BOM in `CLAUDE.md`)

- Arduino Nano 33 IoT (SAMD21 + WiFi NINA, **3.3V logic**)
- PIR HC-SR501 — `OUT → D2`
- (to buy) 12V diaphragm pump + **D4184** MOSFET module (opto-isolated, works from 3.3V)
  + flyback diode 1N5819 + 12V PSU. Everything on a **single 12V rail, common ground**.

## Layout

| Folder | What |
|---|---|
| `no_cats_land/` | 🎯 main firmware — local detection + pump pulse, armable from Home Assistant over WiFi (REST) |
| `test_pir/` | 🔬 motion-sensor test & diagnostics |
| `test_wifi/` | 📶 WiFi test — serves the sensor state at `http://<board-ip>/` |
| `blink/` | 💡 first LED blink smoke-test |

> Each sketch (`.ino`) lives in its own same-named folder — an Arduino requirement.

## Build & upload

```bash
# compile (find the port with: arduino-cli board list)
arduino-cli compile --fqbn arduino:samd:nano_33_iot ./no_cats_land
arduino-cli upload -p /dev/cu.usbmodem1201 --fqbn arduino:samd:nano_33_iot ./no_cats_land
```

## WiFi credentials

`arduino_secrets.h` is **not in the repo** (it holds the password). Both WiFi sketches need
their own copy:

```bash
cp no_cats_land/arduino_secrets.example.h no_cats_land/arduino_secrets.h
cp test_wifi/arduino_secrets.example.h    test_wifi/arduino_secrets.h
# then fill in your SSID + password in each
```

## Home Assistant control (REST)

The main firmware runs detection and the pump **locally** (works even if WiFi/HA is down).
Home Assistant only arms/disarms and monitors, via HTTP:

| Endpoint | Action |
|---|---|
| `GET /status` | JSON `{"armed":bool,"state":"...","motion":0\|1,"shots":n}` |
| `GET /arm`    | arm, returns status |
| `GET /disarm` | disarm, returns status |

The board starts **disarmed**. Give it a fixed address (DHCP reservation on the router),
then wire it into HA with a `rest_command` (arm/disarm), a `rest` sensor (poll `/status`)
and a template `switch`. See `docs/home-assistant.yaml` for a ready-to-paste config.

## PIR calibration

- **Left pot = sensitivity/range**, **right pot = hold time**.
- Both fully left = minimum. Sensitivity ~1/4 right catches motion at 1–2 m.
- Live sensor state: `curl http://<board-ip>/` (returns `MOTION` / `idle`).

## Status

- ✅ sensor wired, verified, roughly calibrated; reports over WiFi
- ✅ firmware: local state machine (warm-up → idle → pulse → cooldown) + REST arm/disarm/status
- ⏳ buy pump/MOSFET/diode/PSU; wire the pump; add to Home Assistant; final mounting
