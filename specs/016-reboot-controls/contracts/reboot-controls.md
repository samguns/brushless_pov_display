# Contract: Reboot Controls

## Scope

This contract extends the existing STA-mode HTTP management portal on port 80.
It preserves the portal's current local access model, single-client limit,
static response buffers, USB update route, and Wi-Fi OTA route.

## `GET /settings`

The System card includes a `Reboot` action describing a normal board restart and
temporary display/network interruption.

- When manual reboot is available, the action links to `GET /reboot`.
- While OTA is receiving, validating, or ready to restart, the action is disabled
  and the page explains that firmware update must finish first.
- Existing USB and Wi-Fi firmware-update actions remain unchanged.

## `GET /reboot`

Serves a normal-reboot confirmation page.

### Available response

- Status: `200 OK`
- Content-Type: `text/html; charset=utf-8`
- Body includes:
  - warning that display and network access will be interrupted;
  - statement that the board returns to normal operating mode;
  - statement that saved Wi-Fi and display settings are preserved;
  - form submitting `POST /reboot`;
  - Cancel link to `/settings`.

### Unavailable response

- Status: `409 Conflict`
- Body explains whether firmware update or another reboot action is active.
- No pending action changes.

## `POST /reboot`

Requests one normal software reboot. The request uses the existing portal access
model and requires no body beyond the confirmed POST.

### Accepted response

- Status: `202 Accepted`
- Content-Type: `text/html; charset=utf-8`
- Body acknowledges that restart was accepted, warns that the connection will
  drop, and tells the user to reopen the management address after startup.
- The normal pending target is staged only after the response is prepared.
- The browser need not remain connected after acceptance.

After the response finishes or the bounded flush deadline expires, the portal
allows its existing short network grace period and initiates a normal watchdog
reboot. Reset begins within five seconds of acceptance. The request does not
enter USB BOOTSEL, invoke firmware installation, or modify saved settings.

### Conflict response

- Status: `409 Conflict`
- Returned when OTA is receiving, validating, ready to restart, or when any
  reboot target is already pending.
- No new reset is staged and no active update is aborted.

Repeated submissions for one confirmation sequence therefore cause at most one
accepted reboot.

## Existing `POST /update`

The existing USB update confirmation continues to stage the mutually exclusive
`usb_bootsel` target and ultimately calls the ROM USB-boot reset. It does not use
the normal watchdog path.

## Revised `GET /`

Overview retains Blink Active/Idle and all network, address, clock, RPM, firmware,
and display-health behavior. It contains no Blink Frequency card, Hz value, or
frequency placeholder.

## Revised `GET /status`

The response retains Wi-Fi status and Blink readiness but removes the obsolete
frequency member.

```json
{
  "wifi": {
    "state": "connected",
    "ip": "192.0.2.10"
  },
  "blink": {
    "active": true
  }
}
```

`blink.frequency_hz` is intentionally no longer part of the contract. Rotation
speed and clock presentation contracts remain unchanged.
