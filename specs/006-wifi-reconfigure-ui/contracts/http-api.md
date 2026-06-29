# Contract: Wi-Fi Reconfiguration HTTP API

**Feature**: 006-wifi-reconfigure-ui

Extends the existing STA management portal (`wifi_sta_http`). Pages are
server-rendered HTML served from flash; no external assets. One client at a time.

## GET / (status page) — extended

- Adds a link/button to the reconfiguration page (`GET /wifi`) alongside the
  existing "Update firmware" action.
- Continues to show current SSID, IP, connectivity state, and blink status.
- MUST NOT display the stored password.

## GET /wifi (reconfiguration page)

- Returns the reconfiguration form:
  - Current SSID shown for context (read-only display).
  - Manual SSID text field (default empty or prefilled with current SSID).
  - Password field, masked, always empty on load.
  - "Scan" button (submits `GET /wifi?scan=1`).
  - Submit button (posts to `POST /config`).
- No authorization required.

## GET /wifi?scan=1 (scan + list)

- Triggers a Wi-Fi scan, waits for completion within a bounded deadline, and
  re-renders the reconfiguration page including a selectable list of nearby
  networks (SSID + secured/open indication), sorted by signal strength.
- Selecting a network populates the SSID field; manual entry remains available.
- If no networks are found, the page indicates so and manual entry remains usable.
- Scanning MUST NOT permanently drop connectivity or corrupt stored credentials;
  it may briefly perturb the active link.

## POST /config (apply new credentials)

- Body: form-encoded `ssid` and `password` (URL-decoded by the handler).
- No authorization required (open endpoint, consistent with `POST /update`).
- Behavior:
  1. **Validate**: SSID non-empty and ≤ 32 chars; password 8–63 chars. On failure
     → respond immediately with the reconfiguration page showing a validation
     error banner; no disconnect/connect performed.
  2. **Stage + acknowledge (deferred apply)**: on valid input, stage the change
     and respond immediately with an "applying" page that tells the owner the link
     will drop and to reconnect to the target network (or the previous one on
     failure). This reply is flushed before any radio switch so the client
     receives feedback (the test-connect drops the current TCP connection).
  3. **Test-connect** (in the runtime loop, after flush): disconnect current,
     attempt the new credentials within a bounded timeout.
  4. **On success**: persist the new credentials (preserving `admin_token`),
     refresh runtime SSID/IP, and restart the portal on the new network. The
     outcome is shown as a success banner on the status/reconfiguration pages.
  5. **On failure**: revert to the previously working credentials, restore
     reachability, and show a failure banner on the status/reconfiguration pages.
- The persisted record is always a complete old-or-new set (atomic erase+write+
  verify); an interrupted write never yields a corrupt record.

## Behavioral Contracts

- **Persistence**: successfully applied credentials are used automatically on the
  next boot.
- **Confidentiality**: the stored password is never returned in any page or status
  response; logs contain lengths only.
- **Resilience**: a failed change leaves the device reachable on its previous
  network (or its existing reconnect/AP-fallback behavior on genuine link loss).
- **Bounded operation**: scan and apply complete within bounded windows; the
  device remains otherwise responsive.

## Observability Contract

Required debug events (USB stdio), without printing the password:
- Reconfiguration page served; scan started/finished with result count.
- Validation rejections (with reason).
- Apply attempt: target SSID, connect success/failure, persist success/failure,
  and revert events.
