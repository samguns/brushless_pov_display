# Research: Wi-Fi Reconfiguration UI

**Date**: 2026-06-29
**Feature**: 006-wifi-reconfigure-ui

## RES-001: Where the reconfiguration UI lives

**Decision**: Add a dedicated reconfiguration page served at `GET /wifi`, linked
from the existing STA status page (`/`). The page shows the current SSID, a manual
SSID field, a masked password field, and a "Scan" button.

**Rationale**:
- The status page (`wifi_sta_web_build_status_page`) currently offers only an
  "Update firmware" action; a dedicated page keeps the status view clean and gives
  room for the scan list and form.
- Server-rendered HTML matches the existing portal approach (self-contained pages
  from flash, no external assets).

**Alternatives considered**:
- Inline form on the status page: rejected — clutters the status view and
  complicates the scan-list rendering.
- Single-page-app with client JS: rejected — heavier than the project's minimal
  server-rendered convention.

## RES-002: Triggering and returning scan results

**Decision**: The "Scan" button issues `GET /wifi?scan=1`. The handler starts a
scan via `wifi_scan_start()`, polls `wifi_scan_is_active()` within a bounded
deadline (servicing `cyw43_arch_poll()`), then renders the reconfiguration page
with the discovered networks (`wifi_scan_get_results`) as selectable entries that
populate the SSID field; manual entry remains available.

**Rationale**:
- Reuses the existing async scan module unchanged (`wifi_scan.*`, ≤20 deduped,
  RSSI-sorted results with `secured` flag).
- Explicit-button trigger (per clarification) avoids perturbing the live link on
  every page view.
- Bounded polling mirrors the existing `wifi_sta_http_poll` reboot-flush pattern,
  keeping behavior predictable.

**Alternatives considered**:
- Separate JSON `GET /scan` consumed by client JS: viable, but server-rendered
  selection avoids added JS and an extra request round-trip.
- Auto-scan on page load: rejected by clarification (connection disruption).

## RES-003: Applying new credentials (test-connect, persist, revert)

**Decision**: Add a `wifi_config` entry point that: (1) keeps the current working
credentials as a backup, (2) disables STA and test-connects to the new SSID/
password with `cyw43_arch_wifi_connect_timeout_ms` (reusing the `try_sta_blocking`
pattern) within a bounded timeout, (3) on success persists via `save_credentials`
(preserving the existing `admin_token`), refreshes runtime SSID/IP, and restarts
the STA portal on the new network, (4) on failure reconnects using the backup
credentials and reports the error.

**Rationale**:
- Mirrors the proven provisioning flow (`ap_provisioning_until_connected` already
  test-connects before `save_credentials`).
- Keeps radio/flash control in `wifi_config`; the HTTP layer only parses and
  reports (Principle III).
- Revert-on-failure satisfies FR-006/SC-003 (device never stranded by a typo).

**Alternatives considered**:
- Persist first, then connect: rejected — a bad password would overwrite the
  working record and could strand the device.
- Fall back to AP provisioning on failure: rejected — the existing reconnect/AP-
  fallback path still applies on real link loss; for an interactive change we
  prefer reverting to the known-good network.

## RES-004: Credential persistence atomicity

**Decision**: Reuse `save_credentials()` which erases the reserved 4 KB sector,
writes the V2 record (magic/version/ssid/password/admin_token/flags/crc32), and
verifies by re-reading. Preserve the current `admin_token` value when writing the
new SSID/password.

**Rationale**:
- The single-sector erase+write+verify with CRC guarantees a complete old-or-new
  record (FR-011); a failed/interrupted write is detected and the prior record (or
  none) governs the next boot.
- No new storage format or sector is needed.

**Alternatives considered**:
- Double-buffered A/B records: rejected — added complexity; the existing verify +
  test-before-commit flow already prevents stranding.

## RES-005: Authorization posture

**Decision**: The change endpoint (`POST /config`) is open — no admin token
required — consistent with the firmware-update endpoint, per the clarification.

**Rationale**:
- Explicit owner decision recorded in the spec; avoids introducing a token-entry
  step in the new UI.

**Alternatives considered**:
- Require the existing admin token: rejected by clarification (convenience), though
  noted as a security tradeoff (anyone on the LAN can repoint the device).

## RES-006: Input validation

**Decision**: Validate before any disconnect/connect: SSID non-empty and ≤ 32
chars; password length 8–63 (WPA2/PSK). Reject invalid input with a clear message
and no radio action. Reuse the existing form-field extraction
(`extract_form_field`) and URL-decode submitted values.

**Rationale**:
- Prevents needless disruption of the live connection on obviously bad input
  (SC-005) and matches existing buffer limits (`WIFI_SSID_MAX_LEN`,
  `WIFI_PASS_MAX_LEN`).

**Alternatives considered**:
- Validate only on the device after connect attempt: rejected — wastes a
  disruptive connect cycle and worsens UX.

## RES-007: Blocking vs the super-loop

**Decision**: Accept bounded blocking during scan and apply. Both run inside the
HTTP request handling on the main core and complete within their bounded windows;
the WS2812 frame is latched/held during the pause and the hall reader resumes
afterward.

**Rationale**:
- Reconfiguration is rare and owner-initiated; a brief, bounded pause is
  acceptable and far simpler than making scan/connect fully asynchronous.
- The existing portal already performs bounded blocking (reboot flush).

**Alternatives considered**:
- Fully async state machine for scan+connect: rejected for this scope; revisit if
  uninterrupted playback during reconfiguration becomes a requirement.

## RES-008: Password confidentiality

**Decision**: Never render the stored password; the password field is always
empty/masked on page load and the value is only accepted on submit. Logs print
lengths, not the password.

**Rationale**:
- Satisfies FR-012/SC-006.

**Alternatives considered**:
- Pre-filling the masked password for editing convenience: rejected — risks
  plaintext exposure and complicates "unchanged password" semantics.
