# Data Model: PIO Blink Concurrent with STA HTTP Server

**Date**: 2026-06-26
**Feature**: 003-pio-blink-sta-server

## Entity: Blink Runtime State

In-memory runtime state for non-blocking blink control.

| Field | Type | Description |
|---|---|---|
| `enabled` | bool | Blink service active/inactive flag |
| `frequency_hz` | uint32 | Target blink frequency |
| `phase_on` | bool | Current blink on/off phase |
| `next_toggle_us` | uint64 | Absolute time for next phase transition |
| `pio_sm` | uint | Active PIO state machine id |
| `pio_program_offset` | uint | Loaded blink program offset |

**Validation rules**:
- `frequency_hz` must be >0 and within implementation-supported range.
- Timing computations must use runtime clock queries, not hardcoded clocks.

## Entity: Connectivity State

Runtime representation of WiFi link/service health.

| Field | Type | Description |
|---|---|---|
| `link_status` | enum (`CYW43_LINK_*`) | Current STA link status |
| `ip_v4` | string (<=15 chars) | Current IPv4 string for status output |
| `last_reconnect_attempt_ms` | uint32 | Last reconnect attempt timestamp |
| `portal_active` | bool | Whether STA portal listener is active |

**State transitions**:
- `CONNECTED` -> `DISCONNECTED`: link drop detected.
- `DISCONNECTED` -> `RECONNECTING`: reconnect attempt started.
- `RECONNECTING` -> `CONNECTED`: reconnect success, portal (re)started.
- `RECONNECTING` -> `AP_FALLBACK`: timeout, AP provisioning path entered.

## Entity: Admin Access Credential

Persisted authorization data for mutating STA endpoints.

| Field | Type | Description |
|---|---|---|
| `token` | char[N] | Shared admin token string |
| `version` | uint8 | Record/schema version for compatibility |
| `crc32` | uint32 | Integrity check for credential payload |

**Validation rules**:
- Token must be non-empty when mutating endpoints are enabled.
- CRC must validate before token use.

## Entity: Auth Throttle State

In-memory rate-limit state for invalid token attempts.

| Field | Type | Description |
|---|---|---|
| `window_start_ms` | uint32 | Current throttle window start |
| `invalid_count` | uint16 | Invalid token attempts in current window |
| `blocked_until_ms` | uint32 | If set, reject mutating requests until this time |

**Behavior rules**:
- Missing/invalid token returns 401 while under threshold.
- Exceeding threshold returns 429 until block window expires.
- Valid token requests remain serviceable during throttle events.

## Memory Notes

- All entities above are static/global or stack-managed with deterministic bounds.
- No heap allocation is introduced in blink or polling hot paths.
- This feature does not require dynamic containers or unbounded request buffering.
