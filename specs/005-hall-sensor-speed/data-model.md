# Data Model: Hall Sensor Rotation Speed

**Date**: 2026-06-29
**Feature**: 005-hall-sensor-speed

## Entity: HallSensorConfig

Static configuration for the Hall input and measurement behavior.

| Field | Type | Description |
|---|---|---|
| `pin` | uint | Input GPIO (default 15 / GP15) |
| `active_low` | bool | True if magnet-present asserts logic low (default true) |
| `pull_up` | bool | Enable internal pull-up (not needed for push-pull output; default false) |
| `debounce_us` | uint32 | Minimum inter-event lockout in microseconds (default ~1000) |
| `magnets_per_rev` | uint8 | Magnet passes per revolution (default 1) |
| `stop_timeout_us` | uint32 | No-event timeout before speed is zeroed/stale (default 1 500 000) |

**Validation rules**:
- `magnets_per_rev` must be ≥ 1.
- `debounce_us` must be less than the minimum legitimate event interval at the
  maximum supported speed.
- `stop_timeout_us` must exceed the maximum legitimate event interval at the
  minimum supported speed.

## Entity: RotationCapture

Interrupt-shared raw capture state. Written by the edge handler, read under a
critical section by the derivation path.

| Field | Type | Description |
|---|---|---|
| `last_edge_us` | uint64 | Timestamp of the most recent accepted edge |
| `prev_edge_us` | uint64 | Timestamp of the edge before last (for interval) |
| `last_interval_us` | uint32 | Most recent accepted inter-event interval |
| `edge_count` | uint32 | Count of accepted edges since init (diagnostics) |
| `has_two_edges` | bool | True once at least two edges have been captured |

**State transitions**:
- On an accepted edge: `prev_edge_us ← last_edge_us`, `last_edge_us ← now`,
  `last_interval_us ← now - prev`, `edge_count++`, set `has_two_edges` after the
  second edge.
- An edge within `debounce_us` of `last_edge_us` is rejected (no state change).

## Entity: RotationMeasurement

Derived, consumer-facing measurement result produced by the read function.

| Field | Type | Description |
|---|---|---|
| `period_us` | uint32 | Revolution period (`interval_us * magnets_per_rev`) |
| `rpm` | float (or uint16) | Spinning speed in revolutions per minute |
| `hz` | float | Spinning speed in revolutions per second |
| `valid` | bool | True when a fresh in-range measurement exists |
| `stale` | bool | True when no edge within `stop_timeout_us` (speed = 0) |
| `last_update_us` | uint64 | Timestamp when this result was computed |

**Validation rules**:
- `valid` is false until `has_two_edges` is true (first revolution unknown,
  edge-case "first revolution after start").
- When `stale` is true, `rpm`/`hz` are reported as 0.
- Out-of-range intervals (faster than max or slower than min supported speed) are
  reported as bounded rather than as impossible values.

## Entity: HallSensorState

Top-level driver instance aggregating the above.

| Field | Type | Description |
|---|---|---|
| `config` | HallSensorConfig | Active configuration |
| `capture` | RotationCapture | Interrupt-shared capture state |
| `initialized` | bool | Driver init/IRQ-registration succeeded |

## Static Memory Budget Notes

- All state is fixed-size; `HallSensorState` is a single static/owned instance.
- No dynamic allocation in the interrupt handler or the read path.
- Estimated static RAM delta is under 64 bytes (timestamps, counters, config,
  flags); the implementation phase records the measured total in validation
  artifacts (spec budget ceiling: 256 bytes).
