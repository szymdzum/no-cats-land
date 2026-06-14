# no-cats-land — HTTP API reference

The board (Arduino Nano 33 IoT) runs a tiny HTTP server on **port 80**. Detection and the
pump pulse run **locally** and keep working even if the network/Home Assistant is down — the
API is only for control, telemetry and tuning.

- **Base URL:** `http://<board-ip>/` (e.g. `http://192.168.1.23/`, DHCP-reserved)
- **Method:** `GET` only · one request per connection
- **Auth:** none (intended for a trusted LAN)
- **Response:** every endpoint returns the **status JSON** (see `/status`) with
  `Content-Type: application/json`
- Arming is also possible over the USB Serial console: send `a` (arm) or `d` (disarm).

## Endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/status` | Current state + telemetry (JSON) |
| GET | `/arm` | Arm the device |
| GET | `/disarm` | Disarm the device |
| GET | `/test` | Fire one manual test pulse (works even when disarmed) |
| GET | `/reset` | Reset the telemetry counters (`shots_total`, `motion_events`) |
| GET | `/set` | Live-tune timings — see query params below |

All endpoints respond with the same status JSON, so a control call also returns the new state.

### `GET /set` — query parameters

Change settings at runtime (no reflash). Any subset of params may be sent. **Stored in RAM —
reset to firmware defaults on reboot.**

| Param | Meaning | Range | Default |
|---|---|---|---|
| `pulse` | pump pulse length (ms) | 50 – 5000 | 500 |
| `cooldown` | pause after a shot (ms) | 0 – 600000 | 15000 |
| `hold` | motion-debounce before firing (ms) | 0 – 10000 | 600 |
| `warmup` | PIR warm-up after boot (ms) | 0 – 120000 | 60000 |
| `maxshots` | safety auto-disarm after N shots/arming (0 = off) | 0 – 1000 | 20 |

Example: `GET /set?pulse=700&cooldown=20000`

## Status JSON

```json
{
  "armed": false,
  "state": "idle",
  "motion": 0,
  "shots_total": 0,
  "motion_events": 0,
  "last_motion_s": -1,
  "last_shot_s": -1,
  "cooldown_left_s": 0,
  "uptime_s": 568,
  "rssi": -76,
  "pulse_ms": 500,
  "cooldown_ms": 15000,
  "hold_ms": 600,
  "warmup_ms": 60000,
  "max_shots": 20,
  "limit_hit": 0
}
```

| Field | Type | Meaning |
|---|---|---|
| `armed` | bool | Whether firing is enabled. Starts `false`. |
| `state` | string | `warmup` \| `idle` \| `active` \| `cooldown` |
| `motion` | 0/1 | Live PIR reading (always `0` during warm-up) |
| `shots_total` | int | Pump activations since last `/reset` |
| `motion_events` | int | Motion onsets (rising edges) since last `/reset`; counted even while disarmed |
| `last_motion_s` | int | Seconds since last motion onset, or `-1` if none yet |
| `last_shot_s` | int | Seconds since last shot, or `-1` if none yet |
| `cooldown_left_s` | int | Seconds remaining in cooldown (`0` unless `state=cooldown`) |
| `uptime_s` | int | Seconds since boot |
| `rssi` | int | WiFi signal in dBm (`0` if not connected) |
| `pulse_ms` | int | Current pulse length (see `/set`) |
| `cooldown_ms` | int | Current cooldown length |
| `hold_ms` | int | Current motion-debounce |
| `warmup_ms` | int | Current warm-up length |
| `max_shots` | int | Safety cap: auto-disarm after N shots/arming (`0` = off) |
| `limit_hit` | 0/1 | `1` after the safety cap auto-disarmed (cleared on next arm) |

## Behaviour notes

- **Starts disarmed on first flash**, then **restores the last armed state from flash** after a
  reboot/power blip. Motion sensing + telemetry run even while disarmed (so the sensor can be
  calibrated remotely); only firing requires `armed = true`.
- **Persistence:** `armed` and `shots_total` are saved to flash and survive reboots. The tunable
  `/set` values reset to firmware defaults on reboot.
- **Debounce:** a shot fires only after motion persists for `hold_ms`.
- **Anti-flood:** after a shot the device enters `cooldown` for `cooldown_ms` and ignores motion.
- **Safety cap:** after `max_shots` shots within one arming, the device auto-disarms and sets
  `limit_hit=1` (tune via `/set?maxshots=`; `0` disables).
- The HTTP server is not served during an active pulse, so a slow request can never extend a
  water shot.

## Examples

```bash
BOARD=http://192.168.1.23

curl $BOARD/status                  # read telemetry
curl $BOARD/arm                     # arm
curl $BOARD/disarm                  # disarm
curl $BOARD/test                    # one manual pulse
curl "$BOARD/set?pulse=700&hold=800"  # tune
curl $BOARD/reset                   # zero counters
```

For the Home Assistant wiring built on top of this API, see
[`home-assistant.yaml`](./home-assistant.yaml).
