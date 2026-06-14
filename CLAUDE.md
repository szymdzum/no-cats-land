# no-cats-land

A water- (and otherwise-) based, motion-triggered cat deterrent. ACME-grade project,
driven by an Arduino Nano 33 IoT. A PIR detects the cat entering an alcove, the
microcontroller fires a short burst from a 12V diaphragm pump.

This document is the project context — read by Claude Code before it touches the firmware.

## Name

`no-cats-land` (a play on "no man's land"). README carries a Maginot Line motto — a
defense that gets bypassed. The name is baked into the project's irony.

## Hardware goal

A short, harmless squirt of water (or mist) when the PIR detects motion in the alcove.
No harm, no spinning blades, no water hitting an already-damp wall.

## Hardware / BOM

Already have:
- Arduino Nano 33 IoT (with headers) — SAMD21 + NINA, **3.3V logic**
- PIR HC-SR501 (HW-416A, BISS0001 chip) — VCC 4.5–20V, OUT 3.3V TTL
- mini breadboard + dupont wires

To buy:
- 12V diaphragm pump (mini, for "water systems") — short pulses, low current
- **D4184 / AOD4184** MOSFET module with opto-isolation (works from 3.3V — NOT IRF520)
- flyback diode 1N5819 (Schottky) or 1N4007
- 12V 2A PSU with a 5.5/2.1 DC plug + screw-terminal DC jack
- silicone tubing ~6mm + small reservoir (a bottle will do)

Alternative actuators (same brain, different "shot"):
- 12V turbine fan/blower (hiss + gust, no water) — safer for a damp alcove
- atomizer/water gun + a servo squeezing the trigger (the most Looney Tunes variant)

## Power architecture

One voltage: **12V for everything.**
- PIR VCC — takes up to 20V, so 12V is fine; OUT is still 3.3V (safe for the Nano)
- Nano — via VIN (range 4.5–21V)
- pump — via the MOSFET module (switched)

**Common ground** — PSU minus, Nano GND, PIR GND, module GND tied to a single point.
This is the most common spot where people trip up.

## Wiring diagram

```
+12V ─────┬──────────┬─────────────┬──────────────┐
          │          │             │              │
        PIR VCC    Nano VIN     MOSFET 12V+   (via diode) Pump +
          │          │             │              │
        PIR OUT ─► Nano D2         │            Pump −
                   Nano D3 ─► MOSFET SIG ──► MOSFET OUT− ─┘
                   Nano 3V3 ─► MOSFET VCC (signal side)
          │          │             │              │
GND ──────┴──────────┴─────────────┴──────────────┘  (common ground)
```

Flyback diode 1N5819 **in parallel with the pump**: cathode (stripe) to +12V, anode to the
switched side (OUT− from the module). The pump is an inductive load; without the diode the
turn-off spike will kill the MOSFET. The D4184 module does not have one onboard.

Having the "Nano in the middle" (rather than PIR straight to the MOSFET) gives a cooldown
and a shot limit, so the pump doesn't fire every second at every warm object (humans included).

## Gotchas (verified, not guessed)

**Nano 33 IoT:**
- I/O is **3.3V and NOT 5V tolerant** — 5V on a GPIO damages the board.
  The PIR OUT is 3.3V, so it wires straight in, no level shifter. Convenient fit.
- The "5V" pin is **dead by default** (0V) — it only comes alive after soldering the VUSB
  jumper and powering via USB. So the whole rig runs on 12V via VIN, not the 5V pin.

**HC-SR501:**
- pin order VCC/OUT/GND is **not standardized** across revisions.
  Middle pin = OUT (certain). The outer pins (VCC/GND) must be confirmed — swapping fries
  the module. Verified on this unit: GND = the outer pin whose trace runs to the negative
  (striped) leg of the electrolytic capacitor.
- H/L jumper: H = retrigger, L = single pulse. For area denial, ultimately **L**.
- ~60s warm-up after power-on before readings are trustworthy.

**MOSFET D4184/AOD4184:**
- has opto-isolation (PC817) — that's why 3.3V from the Nano can drive it.
- signal side (SIG/VCC/GND) + power side (12V+ and switched output).
  Pad names vary (PWM/+/− instead of SIG/VCC/GND) — check the silkscreen on the unit.

**Diaphragm pump:**
- don't run it dry for long (the membrane dislikes it). Short pulses with water are OK.
- some mini pumps prime poorly — reservoir higher / flooded inlet.

### Verified in this session (2026-06-13) — important for phase 1

- **`arduino-cli monitor` does NOT work on this board** — returns 0 bytes (native-USB
  reset-on-open). The test plan below reads the PIR "on Serial" — to actually read that
  Serial, use: `stty -f /dev/cu.usbmodemXXXX 9600 raw -echo` then
  `cat /dev/cu.usbmodemXXXX` (background, sleep, kill). Alternatively report state over
  WiFi (HTTP) — that worked flawlessly (see `test_wifi/`).
- **Read D2 as `INPUT_PULLDOWN`** — a loose OUT wire "floats" and gives false HIGHs; the
  pulldown ties it to 0 when the PIR is quiet. (A loose dupont was the first real bug.)
- **PIR pots on this unit:** left = sensitivity/range, right = hold time. Both fully left =
  minimum; sensitivity ~1/4 right catches motion at 1–2 m.
- **WiFi:** library `WiFiNINA`, module is **2.4 GHz only**. `WiFi.config()` (static IP) was
  flaky — DHCP works. SSID is **case-sensitive** (a typo = "network not found").

## Firmware plan (Nano 33 IoT, Arduino)

Minimal state, no libraries:
- `PIR_PIN = D2` (INPUT) — reads PIR OUT
- `PUMP_PIN = D3` (OUTPUT) — drives the MOSFET module SIG
- ~60s PIR warm-up in `setup()` before entering the loop
- HIGH-edge detection on D2
- on trigger: a short pulse on D3 (e.g. 300–800 ms), then a **cooldown** (e.g. 10–30 s),
  so it doesn't fire in bursts and flood the alcove
- optional shot counter / Serial log in debug mode
- (phase 2, since this is a Nano 33 IoT) WiFi/BLE: remote event log, parameter changes

Field-tunable parameters: pulse length, cooldown length, PIR sensitivity (pot),
H/L jumper setting (L is more convenient for the cooldown logic).

## Test plan (ordered, fewest things that can go wrong first)

1. **PIR only, no 12V and no pump.** Power the PIR from 5V (USB) or, quick and dirty, 3.3V
   from the Nano (below spec, shorter range, but enough for "is it alive"). OUT → D2.
   The sketch reads D2 and prints "MOTION/none" on Serial. Wait out the warm-up, wave a hand.
2. Once the PIR reacts — add the MOSFET module and pump on a **separate 12V**, common ground,
   flyback diode in place. Test the pulse dry first (no water), briefly.
3. Water, reservoir, tubing. Tune pulse length and cooldown.
4. Only then mount it in the final spot.

## Safety / don't hurt yourself

- **Water does not go into a damp alcove.** There's already a leak stain and mold risk there.
  Aim away from the wall, or use the blower/compressed-air variant instead of water.
- First remove old urine enzymatically, otherwise we seal the smell inside.
- Leave ventilation if the alcove hides pipes — sealing it tight encourages moisture.
- The pump does not run dry. The MOSFET always has a flyback diode.
- Don't apply 12V until the PIR's VCC/GND are confirmed.

## Notes for the coding agent

- Target: Arduino Nano 33 IoT (SAMD21). Arduino core: `arduino:samd`.
- Keep the logic in a single `.ino` to start, no external libraries through phase 1.
- Pinout in one place as `#define`s, easy to change.
- All timings (pulse, cooldown, warm-up) as constants at the top of the file.
- Do not assume 5V on any Nano pin. PIR OUT is 3.3V and that's the whole truth.
- Phase 1 must work on the Serial Monitor before anything switches the pump.
- Sketches: `no_cats_land/` (main firmware), `test_pir/`, `test_wifi/`, `blink/`.
  Each `.ino` in its own same-named folder (Arduino requirement).
- Build/upload: `arduino-cli compile --fqbn arduino:samd:nano_33_iot ./<folder>`
  and `arduino-cli upload -p <port> --fqbn arduino:samd:nano_33_iot ./<folder>`.
- Language: chat may be in Polish, but code, comments, docs and commit messages are English.

## Design notes

Safety/robustness patterns already in `no_cats_land/no_cats_land.ino` (phase 2 draft):
- **motion debounce (`MOTION_HOLD_MS`)** — fire only after motion persists (rejects glitches)
- **starts DISARMED + manual arming** (Serial: 'a'/'d') — the pump never fires on power-up
- **`MAX_SHOTS` auto-disarm** — safety cap against a runaway burst
- **short pulse + cooldown** — keep the water shot ~0.5s; never run the actuator for seconds
  (that would flood the alcove)
- **`INPUT_PULLDOWN` on D2** — a loose OUT wire won't float
- **PIR warm-up stays** — HC-SR501 needs ~60s before readings are trustworthy

## Status

- [x] PIR identification (HC-SR501 / BISS0001), pinout verified by trace
- [x] power architecture (single 12V, common ground)
- [x] wiring diagram PIR + Nano + MOSFET + pump + diode
- [ ] buy: pump, D4184 module, diode, 12V PSU, tubing
- [ ] firmware phase 1: read PIR on Serial
- [ ] firmware phase 2: cooldown + pump pulse
- [ ] final mounting
