# no-cats-land

Wodny (i nie tylko) odstraszacz dla kota na czujnik ruchu. Projekt ACME-grade,
sterowany Arduino Nano 33 IoT. PIR wykrywa kota wchodzącego we wnękę, mikrokontroler
odpala krótki strzał z pompki membranowej 12 V.

Dokument jest kontekstem projektu — czytany przez Claude Code zanim dotknie firmware'u.

## Najpierw uczciwie: to jest półśrodek

Kot (kastrowany kocur) **znaczy terytorialnie** — ogon na sztorc, drganie, strzał na
pionową powierzchnię. Przyczyna jest znana: do domu doszła nowa kotka i trwa konflikt
o terytorium. To podręcznikowy wyzwalacz znaczenia u kastrowanych kocurów.

Z tego wynika rzecz, której żaden sprzęt nie obejdzie: **area denial zamyka jedno
miejsce, nie usuwa przyczyny.** Jeśli nie rozładujemy napięcia między kotem a kotką,
on zaznaczy następny punkt przy drzwiach/oknie. Sprzęt to zabawka i domknięcie jednej
wnęki, nie rozwiązanie.

Rozwiązanie właściwe (priorytet, niezależnie od elektroniki):
- zasoby x2, porozdzielane w przestrzeni (kuwety: liczba kotów + 1, osobno)
- pionizacja przestrzeni, drogi ucieczki, żeby koty mogły się mijać bez starcia
- ewentualnie cofnięcie integracji (osobne pokoje, wymiana zapachów, stopniowo)
- Feliway Friends/MultiCat (feromon na konflikt między kotami, nie zwykły)
- enzymatyczne kasowanie starych znaczeń (nigdy amoniak — pachnie jak mocz)
- jeśli po tygodniach nie odpuszcza: behawiorysta / wet o wsparcie farmakologiczne

Reszta dokumentu to ten półśrodek — ale zrobiony porządnie.

## Nazwa

`no-cats-land` (gra z "no man's land"). W README motto o Linii Maginota: obrona,
którą się obeszło — bo kot znajdzie następne miejsce. Nazwa wpisana w ironię projektu.

## Cel sprzętowy

Krótki, nieszkodliwy strzał wody (lub mgiełki) gdy PIR wykryje ruch we wnęce.
Bez krzywdy, bez wirujących ostrzy, bez wody lecącej w zawilgoconą ścianę.

## Hardware / BOM

Co już jest:
- Arduino Nano 33 IoT (with headers) — SAMD21 + NINA, **logika 3,3 V**
- PIR HC-SR501 (HW-416A, układ BISS0001) — VCC 4,5–20 V, OUT 3,3 V TTL
- mini breadboard + przewody dupont

Do dokupienia:
- pompka membranowa 12 V (mini, do "systemów wodnych") — krótkie pulsy, niskoprądowa
- moduł MOSFET **D4184 / AOD4184** z optoizolacją (działa z 3,3 V — NIE IRF520)
- dioda gaśnicza 1N5819 (Schottky) lub 1N4007
- zasilacz 12 V 2 A z wtykiem DC 5,5/2,1 + gniazdo DC na śruby
- wężyk silikonowy ~6 mm + zbiorniczek (może być butelka)

Alternatywne wykonawcze (ten sam mózg, inny "strzał"):
- wentylatorek/dmuchawa turbinowa 12 V (świst + podmuch, bez wody) — do wilgotnej wnęki bezpieczniejsze
- atomizer/pistolet na wodę + serwo ściskające spust (wariant najbardziej Looney Tunes)

## Architektura zasilania

Jedno napięcie: **12 V do wszystkiego.**
- PIR VCC — łyka do 20 V, więc 12 V OK, OUT i tak daje 3,3 V (bezpieczne dla Nano)
- Nano — przez VIN (zakres 4,5–21 V)
- pompka — przez moduł MOSFET (przełączana)

**Masa wspólna** — minus zasilacza, GND Nano, GND PIR, GND modułu spięte w jeden punkt.
To jest najczęstszy punkt, na którym ludzie się przewracają.

## Schemat połączeń

```
+12V ─────┬──────────┬─────────────┬──────────────┐
          │          │             │              │
        PIR VCC    Nano VIN     MOSFET 12V+    (przez diodę) Pompka +
          │          │             │              │
        PIR OUT ─► Nano D2         │           Pompka −
                   Nano D3 ─► MOSFET SIG ──► MOSFET OUT− ─┘
                   Nano 3V3 ─► MOSFET VCC (strona sygnałowa)
          │          │             │              │
GND ──────┴──────────┴─────────────┴──────────────┘  (masa wspólna)
```

Dioda gaśnicza 1N5819 **równolegle do pompki**: katoda (pasek) do +12 V,
anoda do strony przełączanej (OUT− z modułu). Pompka to obciążenie indukcyjne,
bez diody przepięcie przy wyłączaniu uszkodzi MOSFET. Moduł D4184 nie ma jej na pokładzie.

Logika "Nano w środku" (a nie PIR wprost do MOSFET-a) daje cooldown i limit strzałów,
żeby pompka nie waliła co sekundę na każdy ciepły obiekt (w tym na człowieka).

## Gotchas (zweryfikowane, nie zgadywane)

**Nano 33 IoT:**
- I/O jest **3,3 V i NIE 5 V tolerant** — 5 V na pin GPIO uszkadza płytkę.
  OUT z PIR-a to 3,3 V, więc wpina się wprost, bez konwertera. Akurat pasuje.
- Pin "5V" **domyślnie martwy** (0 V) — ożywa dopiero po zlutowaniu zworki VUSB
  i zasilaniu przez USB. Dlatego cały układ jedzie na 12 V przez VIN, nie na pinie 5V.

**HC-SR501:**
- kolejność pinów VCC/OUT/GND **nie jest znormalizowana** między rewizjami.
  Środkowy pin = OUT (pewne). Skrajne (VCC/GND) trzeba potwierdzić — zamiana smaży moduł.
  Weryfikacja na tym egzemplarzu: GND = skrajny pin, którego ścieżka biegnie do minusowej
  (paskowanej) nogi kondensatora elektrolitycznego.
- zworka H/L: H = retrigger, L = pojedynczy impuls. Do area denial docelowo **L**.
- rozgrzewka ~60 s po zasilaniu, zanim odczyty są wiarygodne.

**MOSFET D4184/AOD4184:**
- ma optoizolację (PC817) — dlatego steruje nim 3,3 V z Nano.
- strona sygnałowa (SIG/VCC/GND) + strona mocy (12V+ i przełączane wyjście).
  Nazwy padów bywają różne (PWM/+/− zamiast SIG/VCC/GND) — sprawdzić nadruk egzemplarza.

**Pompka membranowa:**
- nie odpalać na sucho na długo (membrana nie lubi). Krótkie pulsy z wodą OK.
- niektóre mini-pompki słabo zasysają — zbiornik wyżej / zalany wlot.

### Zweryfikowane w tej sesji (2026-06-13) — ważne dla fazy 1

- **`arduino-cli monitor` NIE działa na tej płytce** — zwraca 0 bajtów (native-USB
  reset-on-open). Test plan poniżej zakłada odczyt PIR „na Serial" — żeby ten Serial
  faktycznie przeczytać, użyj: `stty -f /dev/cu.usbmodemXXXX 9600 raw -echo` a potem
  `cat /dev/cu.usbmodemXXXX` (w tle, sleep, kill). Alternatywnie raportuj stan przez
  WiFi (HTTP) — to działało bezbłędnie (patrz `test_wifi/`).
- **Czytaj D2 jako `INPUT_PULLDOWN`** — luźny styk na OUT „pływa" i daje fałszywe HIGH;
  pulldown ściąga go do 0, gdy PIR milczy. (Luźny dupont to był pierwszy realny bug.)
- **Potencjometry PIR na tym egzemplarzu:** lewa = czułość/zasięg, prawa = czas trzymania.
  Obie w lewo = minimum; czułość ~1/4 w prawo łapie ruch z 1–2 m.
- **WiFi:** biblioteka `WiFiNINA`, moduł tylko **2,4 GHz**. `WiFi.config()` (statyczny IP)
  bywał kapryśny — DHCP działa. SSID jest **case-sensitive** (literówka = „nie widzę sieci").

## Plan firmware (Nano 33 IoT, Arduino)

Stan minimalny, bez bibliotek:
- `PIR_PIN = D2` (INPUT) — czyta OUT z PIR
- `PUMP_PIN = D3` (OUTPUT) — steruje SIG modułu MOSFET
- rozgrzewka PIR ~60 s w `setup()` zanim wejdziemy w pętlę
- detekcja zbocza HIGH na D2
- po wyzwoleniu: krótki puls na D3 (np. 300–800 ms), potem **cooldown** (np. 10–30 s),
  żeby nie waliło seriami i nie zalało wnęki
- opcjonalny licznik strzałów / log na Serial w trybie debug
- (faza 2, bo to Nano 33 IoT) WiFi/BLE: zdalny log zdarzeń, zmiana parametrów

Parametry do strojenia w polu: czas pulsu, długość cooldownu, czułość PIR (potencjometr),
ustawienie zworki H/L (do logiki z cooldownem wygodniejsze L).

## Plan testów (kolejność, od najmniej rzeczy które mogą pójść źle)

1. **Sam PIR, bez 12 V i bez pompki.** Zasilanie PIR z 5 V (USB) lub na szybko 3,3 V z Nano
   (poniżej spec, zasięg mniejszy, ale do "czy żyje" wystarczy). OUT → D2.
   Szkic czyta D2 i pisze na Serial "RUCH/brak". Odczekać rozgrzewkę, pomachać ręką.
2. Gdy PIR reaguje — dołożyć moduł MOSFET i pompkę na **osobnym 12 V**, masa wspólna,
   dioda gaśnicza na miejscu. Test pulsu na sucho najpierw bez wody, krótko.
3. Woda, zbiornik, wężyk. Stroić czas pulsu i cooldown.
4. Dopiero potem montaż w docelowym miejscu.

## Safety / nie zrób sobie krzywdy

- **Woda nie leci w zawilgoconą wnękę.** Tam już jest zaciek i ryzyko pleśni.
  Celować od ściany, albo użyć wariantu dmuchawy/sprężonego powietrza zamiast wody.
- Najpierw enzymatycznie skasować stary mocz, inaczej zamkniemy zapach w środku.
- Zostawić wentylację, jeśli wnęka kryje rury — szczelne zamknięcie sprzyja wilgoci.
- Pompka nie chodzi na sucho. MOSFET zawsze z diodą gaśniczą.
- 12 V nie podawać, dopóki VCC/GND PIR-a nie są potwierdzone.

## Notes for the coding agent

- Target: Arduino Nano 33 IoT (SAMD21). Arduino core: `arduino:samd`.
- Trzymaj logikę w jednym `.ino` na start, bez bibliotek zewnętrznych do fazy 1.
- Pinout w jednym miejscu jako `#define`, łatwe do zmiany.
- Wszystkie czasy (puls, cooldown, warm-up) jako stałe na górze pliku.
- Nie zakładaj 5 V na żadnym pinie Nano. OUT z PIR to 3,3 V i to jest cała prawda.
- Faza 1 ma działać na Serial Monitor zanim cokolwiek przełącza pompkę.
- Sketche: `no_cats_land/` (główny firmware), `test_pir/`, `test_wifi/`, `blink/`.
  Każdy `.ino` w osobnym folderze o tej samej nazwie (wymóg Arduino).
- Build/upload: `arduino-cli compile --fqbn arduino:samd:nano_33_iot ./<folder>`
  oraz `arduino-cli upload -p <port> --fqbn arduino:samd:nano_33_iot ./<folder>`.

## Status

- [x] identyfikacja PIR (HC-SR501 / BISS0001), pinout zweryfikowany ścieżką
- [x] architektura zasilania (jedno 12 V, masa wspólna)
- [x] schemat połączeń PIR + Nano + MOSFET + pompka + dioda
- [ ] dokupić: pompka, moduł D4184, dioda, zasilacz 12 V, wężyk
- [ ] firmware faza 1: odczyt PIR na Serial
- [ ] firmware faza 2: cooldown + puls pompki
- [ ] montaż docelowy
- [ ] (równolegle, ważniejsze) rozładować konflikt kot–kotka
