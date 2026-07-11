# Firmware Update HTTP Contract

**Base URL**: `http://<device-ip>` in normal STA mode. Mutation endpoints require the stored
admin token in `X-Admin-Token` or `admin_token`; absent token disables FOTA mutation.

## `GET /update`

Returns an HTML update page with running identity, board ID, maximum package size, warning,
file/token fields, and current/last state.

## `POST /update`

Accepts exactly one `multipart/form-data` part named `firmware`; the server streams content.

| Condition | Response | Result |
|-----------|----------|--------|
| No/invalid authorization | `401` / throttled `429` | No slot write |
| Active update | `409` | Existing session unchanged |
| Missing/empty/non-multipart | `400` | Running firmware retained |
| Oversize/CRC/wrong board | `422` | Slot invalidated; clear reason |
| Timeout/disconnect | connection close | Partial slot discarded |
| Valid package | `202 application/json` | Validate then schedule automatic restart |

The response contains `state`, `received_bytes`, `expected_bytes`, `percent`, `build_id`, and
a plain-language message. A slot is never valid until its complete FOTA SHA-256 check passes.

## `GET /update/status`

Returns safe pollable JSON status during and after upload; it contains no token or secrets.

After a valid payload has passed its complete SHA-256 check, the device reports
`ready_to_restart`, flushes the final status response, marks the FOTA slot valid, and
automatically restarts after a short fixed grace period. The owner does not need to submit a
second request. The bootloader swaps slots; `pov_leds.cpp` commits only after healthy startup,
otherwise the next reboot rolls back.

## `POST /update/usb-recovery`

Requires authorization and returns `202` before the device calls `reset_usb_boot(0, 0)`.
The update page presents it as a fallback-only action; it preserves the existing USB update
mode without mixing it into the normal WiFi package flow.

## Recovery

Pre-install failures keep the running application. A failed candidate boot rolls back to the
last committed image. USB BOOTSEL plus generated USB artifacts remains the documented final
recovery path.
