# no-cats-land 🐱🚱

> *„Najlepiej ufortyfikowana linia obrony jest bezużyteczna, jeśli wróg po prostu ją obejdzie."*
> — duch Linii Maginota, patron tego projektu

Wodny (i nie tylko) odstraszacz kota na czujnik ruchu, sterowany **Arduino Nano 33 IoT**.
PIR wykrywa kota wchodzącego we wnękę → mikrokontroler odpala krótki, nieszkodliwy strzał
z pompki membranowej 12 V.

Nazwa to gra z *no man's land*. I jak Linia Maginota — to obrona, którą kot obejdzie,
bo zaznaczy następne miejsce. **To półśrodek.** Prawdziwe rozwiązanie (konflikt
terytorialny kot–kotka) opisane jest w [`CLAUDE.md`](./CLAUDE.md) i jest ważniejsze niż
cała ta elektronika.

## Sprzęt (skrót — pełny BOM w `CLAUDE.md`)

- Arduino Nano 33 IoT (SAMD21 + WiFi NINA, **logika 3,3 V**)
- PIR HC-SR501 — `OUT → D2`
- (do dokupienia) pompka membranowa 12 V + moduł MOSFET **D4184** (optoizolacja, działa z 3,3 V)
  + dioda gaśnicza 1N5819 + zasilacz 12 V. Wszystko na **jednym 12 V, masa wspólna**.

## Struktura

| Folder | Co to |
|---|---|
| `no_cats_land/` | 🎯 główny firmware (straszak) |
| `test_pir/` | 🔬 test i diagnostyka czujnika ruchu |
| `test_wifi/` | 📶 test WiFi — wystawia stan czujnika na `http://<ip-płytki>/` |
| `blink/` | 💡 pierwszy test migania diodą |

> Każdy program (`.ino`) ma własny folder o tej samej nazwie — tego wymaga Arduino.

## Budowanie i wgrywanie

```bash
# kompilacja (sprawdź port: arduino-cli board list)
arduino-cli compile --fqbn arduino:samd:nano_33_iot ./no_cats_land
arduino-cli upload -p /dev/cu.usbmodem1201 --fqbn arduino:samd:nano_33_iot ./no_cats_land
```

## WiFi — dane logowania

`test_wifi/arduino_secrets.h` **nie jest w repo** (zawiera hasło). Aby zbudować WiFi:

```bash
cp test_wifi/arduino_secrets.example.h test_wifi/arduino_secrets.h
# i wpisz swoją nazwę sieci + hasło
```

## Kalibracja czujnika PIR

- **Lewa śrubka = czułość/zasięg**, **prawa śrubka = czas trzymania sygnału**.
- Obie w lewo = minimum. Czułość ~1/4 w prawo łapie ruch z 1–2 m.
- Stan czujnika na żywo: `curl http://<ip-płytki>/` (zwraca `RUCH` / `spokoj`).

## Status

- ✅ czujnik podłączony, zweryfikowany, wstępnie skalibrowany; raportuje przez WiFi
- ⏳ dokupić pompkę/MOSFET/diodę/zasilacz; firmware faza 1 (PIR na Serial) → faza 2 (puls + cooldown)
- ⚠️ równolegle i ważniejsze: rozładować konflikt kot–kotka (patrz `CLAUDE.md`)
