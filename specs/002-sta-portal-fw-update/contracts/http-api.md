# HTTP API Contract: STA Management Portal

**Date**: 2026-06-25
**Feature**: 002-sta-portal-fw-update
**Server**: Raw-lwIP TCP server on the Pico W at its DHCP-assigned IP, **STA mode only**

---

## Overview

While the device is connected to a WiFi network in STA mode, it serves a small management
interface over plain HTTP/1.1 (no TLS, no authentication) on port 80 at the device's
DHCP-assigned IPv4 address. This server runs only in STA mode; the AP-mode provisioning
server (`wifi_http.c`, feature 001) is stopped before STA serving begins, so there is no
port conflict.

Base URL: `http://<device-IP>` (the IP is printed to USB serial — see FR-005).

---

## Endpoints

### `GET /`

Serves the management/status page.

**Response**:
- Status: `200 OK`
- Content-Type: `text/html; charset=utf-8`
- Body: HTML page containing:
  - The device's current IPv4 address (FR-007).
  - The connected network name / SSID (FR-007).
  - An **"Update firmware"** button/link that navigates to `GET /update` (FR-008).

**Example skeleton**:
```html
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>pov-leds</title></head>
<body>
  <h1>pov-leds device</h1>
  <p>Status: connected</p>
  <p>Network: <strong>{SSID}</strong></p>
  <p>IP address: <strong>{IP}</strong></p>
  <p><a href="/update"><button>Update firmware</button></a></p>
</body>
</html>
```

---

### `GET /update`

Serves the firmware-update confirmation page.

**Response**:
- Status: `200 OK`
- Content-Type: `text/html; charset=utf-8`
- Body: HTML page containing (FR-009):
  - A warning that the device will reboot into USB mass-storage mode.
  - A **"Confirm update"** control that submits `POST /update`.
  - A **"Cancel"** control that returns to `GET /` (FR-011).
  - A visible **countdown from 60 seconds**; on reaching 0 with no action, the page
    redirects to `/` (client-side). No reboot occurs unless the user confirms (FR-013).

**Example skeleton**:
```html
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Confirm update</title></head>
<body>
  <h1>Firmware update</h1>
  <p><strong>Warning:</strong> the device will reboot into USB drive mode.
     Drag a new <code>.uf2</code> file onto the drive that appears.</p>
  <p>Returning to normal mode in <span id="c">60</span> s if no action taken.</p>
  <form method="POST" action="/update">
    <button type="submit">Confirm update</button>
  </form>
  <p><a href="/">Cancel</a></p>
  <script>
    var n=60,e=document.getElementById('c');
    setInterval(function(){n--;e.textContent=n;if(n<=0)location.href='/';},1000);
  </script>
</body>
</html>
```

---

### `POST /update`

Confirms the firmware update. Triggers the reboot into USB mass-storage (BOOTSEL) mode.

**Request**:
- Content-Type: `application/x-www-form-urlencoded` (body may be empty; the POST itself is
  the confirmation).

**Response** (sent **before** the reboot, so the browser receives a reply):
- Status: `200 OK`
- Content-Type: `text/html; charset=utf-8`
- Body: a brief "Rebooting into update mode…" page.

**After the response is flushed**:
1. The device waits ~600 ms for lwIP to flush the TCP write and close the connection.
2. The device calls `reset_usb_boot(0, 0)` and enters USB MSD mode within 3 s of
   confirmation (FR-010, SC-004). It does **not** return to the application.
3. The host sees the `RPI-RP2` drive; dropping a UF2 reflashes and reboots the device,
   which then auto-connects via the preserved credentials (FR-001/FR-002).

---

## Error & Edge Responses

| Condition | Behaviour |
|-----------|-----------|
| Request to an unknown path | `302 Found` redirect to `/` (mirrors AP-mode server behaviour). |
| WiFi link drops while serving | Device attempts silent reconnect (≤15 s). On failure, falls back to AP provisioning mode (FR-014). The portal stops while disconnected. |
| Two clients click "Update firmware" | Single-client server; the first confirmed `POST /update` wins and reboots. A second concurrent request is aborted (mirrors `wifi_http.c` single-client handling). |
| User takes no action on `/update` for 60 s | Client-side redirect to `/`; device stays in STA mode (no reboot). |

---

## Constraints

- HTTP/1.1 only. No HTTPS/TLS. No cookies/sessions/authentication (per spec clarification —
  the confirmation step is the only safeguard).
- Single concurrent client (lwIP raw-TCP, one `s_client_pcb` at a time).
- All pages served from static flash buffers; no dynamic allocation in the serve path.
- The `POST /update` response must be sent and the connection closed before
  `reset_usb_boot()` is called.
- Served on port 80 in STA mode only; the AP-mode server is stopped first, so the two never
  bind the port simultaneously.
