# Data Model: WS2812 POV Hello Demo

**Date**: 2026-06-26
**Feature**: 004-ws2812-pov-hello

## Entity: LedStripConfig

Runtime configuration for WS2812 strip driving.

| Field | Type | Description |
|---|---|---|
| `configured_count` | uint8 | Requested LED count from config path |
| `active_count` | uint8 | Bounded count used by renderer (max 57) |
| `is_bounded` | bool | Indicates clamp/reject occurred |
| `max_count` | uint8 | Constant upper bound (=57) |

**Validation rules**:
- `active_count` must be in range 1..57 for normal rendering.
- Values above 57 must be bounded safely before frame write.

## Entity: PovDemoSequence

Immutable sequence metadata for demo text.

| Field | Type | Description |
|---|---|---|
| `chars` | char[5] | Ordered sequence: H,e,l,l,o |
| `length` | uint8 | Sequence length (=5) |
| `duration_ms` | uint16 | Per-character hold duration (=1000 ms) |
| `loop_enabled` | bool | Continuous replay flag |

## Entity: PovPlaybackState

Mutable runtime state for progression through the demo.

| Field | Type | Description |
|---|---|---|
| `current_index` | uint8 | Active character position |
| `started` | bool | Playback has started after init |
| `last_transition_ms` | uint32 | Timestamp of most recent character change |
| `cycles_completed` | uint32 | Full Hello loops completed |

**State transitions**:
- `INIT` -> `RUNNING` when WS2812 output-path readiness succeeds.
- `RUNNING` stays active, increments index every 1000 ms, wraps from index 4 to 0.

## Entity: OutputHealthState

Operational status and recoverable error reporting.

| Field | Type | Description |
|---|---|---|
| `driver_ready` | bool | WS2812 output-path init status |
| `last_error_code` | enum/int | Most recent bounded/recoverable error |
| `last_error_ms` | uint32 | Timestamp for last error event |
| `transition_log_count` | uint32 | Observed transition events for validation |

## Static Memory Budget Notes

- LED frame buffer is statically bounded to the maximum strip length (57 pixels).
- Playback state and sequence metadata are fixed-size structures.
- No dynamic allocation is introduced in display hot paths or transition scheduling.
- Implementation phase must record concrete byte totals in validation artifacts.
