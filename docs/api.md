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

Change timings at runtime (no reflash). Values are **milliseconds**, clamped to the ranges
below. Any subset of params may be sent. **Stored in RAM — reset to firmware defaults on
reboot.**

| Param | Meaning | Range (ms) | Default |
|---|---|---|---|
| `pulse` | pump pulse length | 50 – 5000 | 500 |
| `cooldown` | pause after a shot | 0 – 600000 | 15000 |
| `hold` | motion-debounce before firing | 0 – 10000 | 600 |
| `warmup` | PIR warm-up after boot | 0 – 120000 | 60000 |

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
  "warmup_ms": 60000
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

## Behaviour notes

- **Starts DISARMED.** Motion sensing + telemetry still run while disarmed (so the sensor can
  be calibrated remotely); only firing requires `armed = true`.
- **Debounce:** a shot fires only after motion persists for `hold_ms`.
- **Anti-flood:** after a shot the device enters `cooldown` for `cooldown_ms` and ignores motion.
- **Safety cap:** after `MAX_SHOTS` (20) shots within one arming, the device auto-disarms.
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
