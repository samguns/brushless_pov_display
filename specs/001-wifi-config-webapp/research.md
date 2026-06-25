# Research: WiFi Configuration Web App

**Date**: 2026-06-25
**Feature**: 001-wifi-config-webapp

---

## RES-001: HTTP Server Approach on Pico W

**Decision**: Use lwIP `httpd` (bundled with Pico SDK via `pico_lwip_http`) with CGI handlers for form submission and SSI (Server-Side Includes) for dynamic scan results.

**Rationale**:
- lwIP `httpd` is already present in the SDK; no additional dependencies needed.
- CGI handlers map a POST URL to a C callback function — clean separation between HTTP mechanics and application logic.
- SSI allows the device to inject scan results into the HTML template at serve-time without a JavaScript fetch round-trip, keeping the page functional in minimal browsers.
- Alternative (raw TCP) requires hand-writing HTTP/1.1 parsing and response formatting — significant complexity with no benefit for this use case.
- Alternative (picow_tcp_server example pattern) is lower-level and suitable for binary protocols, not HTML serving.

**Alternatives considered**:
- Raw lwIP TCP: rejected — too much boilerplate for HTTP.
- Mongoose embedded web server: rejected — third-party dependency, licensing overhead, not in SDK.
- REST + JavaScript fetch: rejected — requires JS-capable browser; SSI works in any browser including minimal mobile ones.

**Implementation note**: The HTML file must be converted to a C array using the lwIP `makefsdata` tool (`pico-sdk/lib/lwip/src/apps/http/makefsdata`) or embedded as a C string literal. The latter is simpler for a single-page UI.

---

## RES-002: WiFi Credential Persistence in Flash

**Decision**: Reserve the last 4 KB sector of the 2 MB flash for a fixed-layout credential record. Read/write using `hardware/flash.h` (`flash_range_erase`, `flash_range_program`). Validate with a magic number.

**Rationale**:
- The Pico SDK has no built-in NVS/key-value store. Direct flash sector use is the canonical approach shown in pico-examples.
- A single 4 KB sector (the minimum erasable unit on RP2040) is sufficient for one SSID (≤32 bytes) + one password (≤63 bytes) + metadata.
- The last sector avoids colliding with program code and UF2 metadata.
- A `magic` field (e.g., `0xC0FFEEUL`) distinguishes valid-written records from erased flash (which reads as `0xFF` bytes).
- Flash writes must run from RAM (not flash); the SDK provides `__no_inline_not_in_flash_func` for this. Interrupts must be disabled during erase/write.

**Flash layout** (last 4 KB sector, offset `PICO_FLASH_SIZE_BYTES - 4096`):
```
Offset  Size  Field
0       4     magic (0xC0FFEE01 = valid)
4       33    ssid (null-terminated, max 32 chars + NUL)
37      64    password (null-terminated, max 63 chars + NUL)
101     1     flags (reserved, set to 0)
102     4     crc32 (optional integrity check over bytes 0–101)
106     ...   padding to 4096
```

**Alternatives considered**:
- FatFS on flash: rejected — overkill for two string fields; adds 10+ KB of code.
- littlefs: rejected — same objection; adds wear-levelling complexity beyond requirements.
- EEPROM emulation: not available on RP2040 without external hardware.

---

## RES-003: WiFi Network Scanning (CYW43 Driver)

**Decision**: Use `cyw43_arch_wifi_scan()` (or `cyw43_wifi_scan()` directly) with a result callback. Collect up to N results into a static array, stop when scan completes or array is full.

**Rationale**:
- The CYW43 driver exposes `cyw43_wifi_scan()` which calls a user-provided callback for each discovered BSS.
- Each result includes: SSID (up to 32 bytes), BSSID (6 bytes), RSSI (int16), channel (uint8), auth_mode.
- The scan is asynchronous; the firmware must poll `cyw43_wifi_scan_active()` or use an event to know when it is complete.
- Results are deduplicated by SSID at the application level (multiple APs with the same SSID are shown as one entry, strongest RSSI wins) to keep the UI simple. If BSSID-level selection is needed in a future revision, the data model already captures the BSSID.
- Maximum scan results stored: 20 (static array). Beyond 20, additional results are dropped.

**Alternatives considered**:
- Blocking scan loop: not possible — scan is inherently asynchronous in the CYW43 driver.
- BSSID-level deduplication off: rejected for v1 — duplicate SSIDs confuse non-technical users. Future revision can expose BSSID toggle.

---

## RES-004: AP Mode ↔ STA Mode Transition — Critical Constraint

**Decision**: The CYW43 chip on Pico W does NOT support simultaneous AP + STA (no concurrent dual-mode). Connection validation requires dropping the AP, attempting STA, and if it fails, restarting the AP.

**Rationale**:
This is the most important architectural constraint for the feature:

1. Device starts in AP mode → user connects and opens the configuration page.
2. User submits SSID + password → device receives the POST over the AP link.
3. Device sends a "Connecting…" response page **before** dropping the AP.
4. Device stops AP, switches to STA, attempts connection to target network.
5a. **Success**: Connection established → save credentials to flash → reboot into STA mode. The AP disappears. User is told in the response page to reconnect to their home WiFi.
5b. **Failure**: Connection attempt times out (15 s) or auth rejected → restart AP (same SSID/password as before) → user reconnects to AP and sees an error page on next navigation.

**UX implication**: The user's browser connection to the device's AP is severed the moment the device drops AP (step 4). The "Connecting…" response page must be fully self-contained and instructional — it cannot dynamically update from the server. The page should say: "The device is attempting to connect. If this AP disappears, the connection succeeded. If it reappears within 30 seconds, the connection failed — reconnect and try again."

**Alternatives considered**:
- AP+STA simultaneous: not supported by CYW43439 firmware on Pico W.
- WebSocket for live status: impossible once AP drops.
- Redirect to home-network IP after STA: elegant but unreliable (device doesn't know the user's browser IP to redirect to itself on the new network). Feasible only if mDNS is added (out of scope).

---

## RES-005: Captive Portal / DNS Redirect

**Decision**: Run a simple DNS responder in AP mode that answers all queries with the device's own IP (`192.168.4.1`), triggering the captive portal mechanism in most mobile OSes.

**Rationale**:
- When a device connects to an AP that has no internet, iOS, Android, and Windows automatically probe known URLs (e.g., `connectivitycheck.gstatic.com`). If the response is not the expected internet response, the OS shows a "Sign in to network" prompt that opens a browser.
- By responding to all DNS queries with `192.168.4.1`, the device hijacks these probes and causes the captive portal browser to open automatically on the configuration page.
- The lwIP DNS server (`pico_lwip_dns`) can be configured with a wildcard response.
- This resolves the spec edge case: "does the configuration page appear automatically?"

**Alternatives considered**:
- No DNS responder: user must manually type `192.168.4.1` — adds friction and is non-obvious.
- mDNS only: only works on some platforms and doesn't trigger the captive portal flow.
