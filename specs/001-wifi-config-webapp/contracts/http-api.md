# HTTP API Contract: WiFi Configuration Web App

**Date**: 2026-06-25
**Feature**: 001-wifi-config-webapp
**Server**: lwIP httpd running on the Pico W at `192.168.4.1` (AP mode only)

---

## Overview

The device serves a minimal HTTP/1.1 interface while in AP mode. All endpoints are plain HTTP (no TLS). The interface is designed for basic browsers including mobile devices with no JavaScript requirement.

Base URL: `http://192.168.4.1`

---

## Endpoints

### `GET /`

Serves the WiFi configuration HTML page.

**Triggers**: A WiFi scan runs (or its cached results are used) to populate the network list.

**Response**:
- Status: `200 OK`
- Content-Type: `text/html`
- Body: Full HTML page containing:
  - A `<select>` element listing discovered networks (SSID + signal strength indicator), sorted strongest-first.
  - An always-visible plain text field labelled "Or enter network name manually:" for hidden networks that do not appear in the scan list. No JavaScript is required; the field is always rendered.
  - A password `<input type="password">` field.
  - A "Connect" submit button.
  - A "Refresh networks" link that reloads the page (re-triggers scan).

**SSI tags** (resolved by lwIP httpd at serve time):
| Tag | Replaced with |
|-----|---------------|
| `<!--#networks-->` | `<option>` elements for each scan result |
| `<!--#scan_count-->` | Integer count of discovered networks |

**Example HTML skeleton**:
```html
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>WiFi Setup</title></head>
<body>
  <h1>WiFi Setup</h1>
  <form method="POST" action="/connect">
    <label>Select network:
      <select name="ssid">
        <!--#networks-->
      </select>
    </label>
    <label>Or enter network name manually:
      <input type="text" name="ssid_manual" maxlength="32" placeholder="Hidden network SSID">
    </label>
    <label>Password:
      <input type="password" name="password" maxlength="63">
    </label>
    <button type="submit">Connect</button>
  </form>
  <p><a href="/">Refresh networks</a> (<!--#scan_count--> found)</p>
</body>
</html>
```

---

### `POST /connect`

Submits the selected/entered SSID and password. Triggers the AP→STA transition sequence.

**Request**:
- Content-Type: `application/x-www-form-urlencoded`
- Body fields:

| Field | Type | Max length | Description |
|-------|------|-----------|-------------|
| `ssid` | string | 32 chars | SSID selected from the scan list. Empty if user did not select from the list. |
| `ssid_manual` | string | 32 chars | Manually entered SSID. Used when `ssid` is empty (hidden network). |
| `password` | string | 63 chars | Network password. Empty string is valid (open networks). |

**SSID resolution**: The server uses `ssid` if non-empty; otherwise uses `ssid_manual` if non-empty; otherwise rejects with 400. This requires no JavaScript on the client side.

**Validation (server-side, before connection attempt)**:
- Effective SSID (resolved from `ssid` or `ssid_manual`) must be 1–32 non-empty characters.
- If validation fails → respond with `400 Bad Request` and an HTML error page. No connection attempt is made.

**Success response** (after a valid submission, before AP drops):
- Status: `200 OK`
- Content-Type: `text/html`
- Body: HTML page with the following message:

```html
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>Connecting…</title></head>
<body>
  <h1>Connecting…</h1>
  <p>The device is now attempting to connect to <strong>{SSID}</strong>.</p>
  <p>This setup network (<em>pov-leds-setup</em>) will disappear if the
     connection succeeds. Reconnect your device to your home WiFi.</p>
  <p>If <em>pov-leds-setup</em> reappears within 30 seconds, the connection
     failed. Reconnect to it and try again.</p>
</body>
</html>
```

**After the response is sent**:
1. Device stops AP.
2. Device attempts STA connection to the submitted credentials (15 s timeout).
3a. Success → `save_credentials()` called; if write succeeds → reboot. AP does not reappear.
3b. Success but flash write fails → restart AP, serve "Storage write failed" error page (FR-014).
3c. Connection failure → restart AP (`pov-leds-setup` / `12345678`) → on next `GET /`, an error banner is shown.

**Error state page** (served on `GET /` after a failed connection attempt):
- Same as the normal `GET /` page but with an error banner:
  - Auth failure: "Incorrect password. Please try again."
  - Timeout: "Network not found or out of range. Move closer to your router and try again."
  - Save failure: "Connected but failed to save settings. Please try again."

---

### `GET /scan` *(optional, progressive enhancement)*

Returns a JSON array of currently discovered networks. Useful if JavaScript is available and the page wants to refresh the network list without a full reload. May be omitted in v1 if SSI-based approach is sufficient.

**Response**:
- Status: `200 OK`
- Content-Type: `application/json`
- Body:

```json
[
  { "ssid": "MyHomeWiFi",  "rssi": -45, "secured": true },
  { "ssid": "Neighbours",  "rssi": -72, "secured": true },
  { "ssid": "GuestNet",    "rssi": -80, "secured": false }
]
```

**Fields**:
| Field | Type | Description |
|-------|------|-------------|
| `ssid` | string | Network name |
| `rssi` | integer | Signal strength in dBm (negative) |
| `secured` | boolean | `true` if WPA/WPA2/WPA3; `false` if open |

---

### DNS Wildcard (captive portal)

A DNS server runs on port 53 (UDP) alongside the HTTP server while in AP mode.

**Behaviour**: All DNS A-record queries return `192.168.4.1`, regardless of the queried hostname.

**Effect**: Triggers the captive portal mechanism on iOS, Android, and Windows — the OS automatically opens the configuration page in a browser overlay without the user manually navigating.

---

## Error Responses

| Condition | HTTP Status | Response |
|-----------|-------------|----------|
| SSID empty or missing | 400 | HTML error page |
| SSID > 32 chars | 400 | HTML error page |
| Password > 63 chars | 400 | HTML error page |
| Flash write failure after successful connect | 200 (Connecting page sent) then AP restart | Error banner on next GET / |
| Request to unknown path | 404 | Redirect to `/` |

---

## Constraints

- HTTP/1.0 or HTTP/1.1 only. No HTTPS/TLS.
- Maximum concurrent connections: 1 (lwIP httpd default on embedded target). This prevents race conditions if two browsers are connected to the AP simultaneously — a second POST /connect is queued or dropped until the first completes.
- Maximum request body size: 256 bytes (sufficient for SSID + password fields).
- No cookies, sessions, or authentication on the web interface itself.
- No JavaScript requirement: all page functionality (network list, manual entry field, error banners) is rendered server-side via SSI. The always-visible manual SSID text field removes the need for any client-side show/hide logic.
- The `/connect` response must be sent and the TCP connection closed before the AP is stopped; failure to do so will leave the browser without a response.
