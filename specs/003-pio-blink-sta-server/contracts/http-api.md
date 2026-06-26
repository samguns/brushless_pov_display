# HTTP Contract: STA Portal + Admin Authorization

**Feature**: 003-pio-blink-sta-server

## Scope

Defines STA-mode HTTP behavior relevant to concurrent blinking and admin-protected mutating/configuration endpoints.

## Endpoints

### `GET /`
- Purpose: Read-only status page.
- Auth: Not required.
- Response: `200 text/html` with connectivity and blink status summary.

### `GET /status`
- Purpose: Machine-readable status.
- Auth: Not required.
- Response: `200 application/json`.

Example response:
```json
{
  "wifi": {"state": "connected", "ip": "192.168.1.42"},
  "blink": {"active": true, "frequency_hz": 3}
}
```

### `POST /config` (mutating/config)
- Purpose: Update mutable runtime/config values (including admin token management when enabled by implementation policy).
- Auth: Required (`X-Admin-Token` header or equivalent configured field).
- Responses:
  - `200` on successful update.
  - `401 application/json` if token missing/invalid.
  - `429 application/json` if invalid-attempt throttle exceeded.

Current response on success:
```json
{"result":"ok"}
```

Unauthorized body format:
```json
{"error":"unauthorized"}
```

Throttled body format:
```json
{"error":"too_many_invalid_attempts"}
```

### `POST /update` (mutating/config)
- Purpose: Trigger firmware update transition flow.
- Auth: Required.
- Responses:
  - `200` confirmation/transition response.
  - `401` missing/invalid token.
  - `429` throttled due to repeated invalid attempts.

## Authorization Rules

- Read-only endpoints remain open in STA mode.
- Mutating/config endpoints require valid shared admin token.
- Missing/invalid token responses MUST be 401 with concise JSON error body.
- Invalid-token bursts MUST trigger throttling and return 429 on exceed.
- Typical throttle profile: 5 invalid attempts in a 10 s window triggers a 30 s block.

## Availability and Concurrency Rules

- Request processing must not stop blink runtime service.
- During reconnect attempts, status endpoints should reflect transition state when available.
- After successful reconnect, endpoints recover without reboot.
