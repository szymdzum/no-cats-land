# no-cats-land 🐱🚱

> *"The best-fortified line of defense is useless if the enemy simply walks around it."*
> — the spirit of the Maginot Line, patron of this project

A water- (and otherwise-) based, motion-triggered cat deterrent driven by an
**Arduino Nano 33 IoT**. A PIR sensor detects the cat entering an alcove → the
microcontroller fires a short, harmless burst from a 12V diaphragm pump.

The name plays on *no man's land*. And like the Maginot Line, this is a defense the cat
will walk around — it'll just mark the next spot. **It's a half-measure.** The real fix
(the territorial conflict between the tomcat and the new female cat) is described in
[`CLAUDE.md`](./CLAUDE.md) and matters more than any of this electronics.

## Hardware (summary — full BOM in `CLAUDE.md`)

- Arduino Nano 33 IoT (SAMD21 + WiFi NINA, **3.3V logic**)
- PIR HC-SR501 — `OUT → D2`
- (to buy) 12V diaphragm pump + **D4184** MOSFET module (opto-isolated, works from 3.3V)
  + flyback diode 1N5819 + 12V PSU. Everything on a **single 12V rail, common ground**.

## Layout

| Folder | What |
|---|---|
| `no_cats_land/` | 🎯 main firmware (the deterrent) |
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

`test_wifi/arduino_secrets.h` is **not in the repo** (it holds the password). To build the
WiFi sketch:

```bash
cp test_wifi/arduino_secrets.example.h test_wifi/arduino_secrets.h
# then fill in your SSID + password
```

## PIR calibration

- **Left pot = sensitivity/range**, **right pot = hold time**.
- Both fully left = minimum. Sensitivity ~1/4 right catches motion at 1–2 m.
- Live sensor state: `curl http://<board-ip>/` (returns `MOTION` / `idle`).

## Status

- ✅ sensor wired, verified, roughly calibrated; reports over WiFi
- ⏳ buy pump/MOSFET/diode/PSU; firmware phase 1 (PIR → Serial) → phase 2 (pulse + cooldown)
- ⚠️ in parallel and more important: defuse the cat–cat conflict (see `CLAUDE.md`)
