# Quickstart & Validation: PIO Blink Concurrent with STA HTTP Server

**Date**: 2026-06-26
**Feature**: 003-pio-blink-sta-server

## Prerequisites

- Raspberry Pi Pico W with USB serial access.
- Build environment configured for Pico SDK 2.2.0.
- Known WiFi credentials already provisioned (or provision through existing AP flow first).
- Browser on same LAN as device.

## Build

```bash
ninja -C build
```

Expected: successful build and `build/pov_leds.uf2` output.

## Flash

```bash
picotool load build/pov_leds.uf2 -fx
```

## Validation Scenario 1: Concurrent operation (primary)

1. Boot device into STA-connected mode.
2. Observe LED blink for 60 seconds.
3. Repeatedly load status page (`/` or `/status`) from browser.

Expected:
- Blink does not visibly freeze.
- Status responses succeed for normal request rate.
- Logs show STA connected and active IP.

## Validation Scenario 2: Reconnect continuity

1. With blink + STA HTTP active, temporarily disrupt WiFi.
2. Keep observing blink behavior.
3. Restore WiFi network.

Expected:
- Blink remains active during reconnect attempt.
- HTTP recovers without device reboot after reconnect.
- If reconnect timeout is reached, fallback behavior matches existing AP provisioning flow.

## Validation Scenario 3: Authorization + throttling

1. Call mutating endpoint without token:
	```bash
	curl -i -X POST http://<device-ip>/config
	```
2. Call mutating endpoint with invalid token repeatedly:
	```bash
	for i in $(seq 1 6); do curl -i -X POST -H "X-Admin-Token: bad" http://<device-ip>/config; done
	```
3. Call status endpoint without token (must remain open):
	```bash
	curl -i http://<device-ip>/status
	```
4. Call mutating endpoint with valid token.

Expected:
- Missing/invalid token returns `401` with concise JSON error.
- Excess invalid attempts return `429`.
- `GET /status` remains accessible without token.
- Valid token requests continue to succeed.

## Validation Scenario 4: Provisioning regression check

1. Clear credentials and reboot.
2. Verify AP provisioning flow still works.
3. Provision credentials/token, reboot into STA.

Expected:
- Existing AP flow behavior unchanged.
- On STA boot, concurrent blink + HTTP is active.

## References

- Spec: [spec.md](spec.md)
- Plan: [plan.md](plan.md)
- Research: [research.md](research.md)
- Data model: [data-model.md](data-model.md)
- HTTP contract: [contracts/http-api.md](contracts/http-api.md)
