# antysik 🐱🚫

Odstraszacz kota na **Arduino Nano 33 IoT** — projekt do nauki IoT.

Czujnik ruchu (PIR) wykrywa kota wchodzącego do pomieszczenia, a płytka
uruchamia „straszak" (dmuchawa / silniczek), żeby grzecznie przegonić kota.

## Sprzęt

- **Arduino Nano 33 IoT** (SAMD21 + WiFi NINA)
- **Czujnik ruchu PIR HC-SR501** — `OUT → D2`, `VCC → 3V3` (test) / 5V (docelowo), `GND → GND`
- (Etap 2) **MOSFET logic-level** (np. IRLZ44N / AO3400) sterujący silniczkiem/dmuchawą + osobne zasilanie

## Struktura

| Folder | Co to |
|---|---|
| `antysik/` | 🎯 właściwy projekt — straszak |
| `test_pir/` | 🔬 test i diagnostyka czujnika ruchu |
| `test_wifi/` | 📶 test WiFi — wystawia stan czujnika na `http://<ip-płytki>/` |
| `blink/` | 💡 pierwszy test migania diodą |

> Każdy program (`.ino`) ma własny folder o tej samej nazwie — tego wymaga Arduino.

## Budowanie i wgrywanie

```bash
# kompilacja
arduino-cli compile --fqbn arduino:samd:nano_33_iot ./test_wifi

# wgranie (sprawdź port: arduino-cli board list)
arduino-cli upload -p /dev/cu.usbmodem1201 --fqbn arduino:samd:nano_33_iot ./test_wifi
```

## WiFi — dane logowania

Plik `test_wifi/arduino_secrets.h` **nie jest w repo** (zawiera hasło).
Aby zbudować program WiFi:

```bash
cp test_wifi/arduino_secrets.example.h test_wifi/arduino_secrets.h
# i wpisz swoją nazwę sieci + hasło
```

## Kalibracja czujnika PIR

- **Lewa śrubka = czułość/zasięg**, **prawa śrubka = czas trzymania sygnału**.
- Obie w lewo = minimum. Czułość ~1/4 w prawo łapie ruch z 1–2 m.
- Stan czujnika na żywo: `curl http://<ip-płytki>/` (zwraca `RUCH` / `spokoj`).

## Status

- ✅ Etap 1: czujnik podłączony, zweryfikowany, wstępnie skalibrowany; raportuje przez WiFi.
- ⏳ Do zrobienia: dokończyć kalibrację, Etap 2 (silniczek/dmuchawa przez MOSFET).
