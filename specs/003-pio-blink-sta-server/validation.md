# Validation Log: 003-pio-blink-sta-server

## Environment

- Date: 2026-06-26
- Firmware target: Raspberry Pi Pico W
- Build command: `ninja -C build`

## Scenario 1: Concurrent blink + STA web requests

- Status: Pending hardware run
- Evidence: Firmware now uses a non-blocking main superloop (`wifi_config_runtime_step` + `blink_runtime_step`) and exposes `/status` for repeated request checks.
- Success ratio target: >=95% for repeated `GET /` + `GET /status` over 60 s.

## Scenario 2: Reconnect continuity

- Status: Pending hardware run
- Evidence: Runtime uses reconnect state machine (`DISCONNECTED -> RECONNECTING -> CONNECTED`, timeout to `AP_FALLBACK`) with periodic non-blocking join attempts.

## Scenario 3: Auth + throttling

- Status: Pending hardware run
- Evidence:
  - Mutating endpoints (`POST /config`, `POST /update`) require token.
  - Missing/invalid token returns `401 {"error":"unauthorized"}`.
  - Repeated invalid attempts enter block window and return `429 {"error":"too_many_invalid_attempts"}`.

## Scenario 4: AP provisioning regression

- Status: Pending hardware run
- Evidence: AP provisioning flow remains in place and now accepts admin token while preserving previous SSID/password path.

## Build Verification

- Status: Pass (host build)
- Command: `ninja -C build`
- Result: Link completed successfully (`[6/6] Linking CXX executable pov_leds.elf`).
- Date: 2026-06-26

## Feature Completion Summary

- Runtime architecture: Non-blocking superloop with concurrent blink and WiFi runtime stepping.
- Security behavior: Token-protected mutating endpoints with throttle and explicit 401/429 JSON responses.
- Persistence: Versioned credential schema (legacy-compatible read) including persisted admin token.
- Residual risks: Hardware-only timing/throughput criteria still require on-device validation.
