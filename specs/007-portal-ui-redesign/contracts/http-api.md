# Contract: Management Portal HTTP API (UI Redesign)

**Feature**: 007-portal-ui-redesign

Extends/re-skins the existing STA management portal (`wifi_sta_http` /
`wifi_sta_web`). Pages are server-rendered HTML served from flash, fully
self-contained (one shared compact CSS block, minimal inline JS, inline SVG
icons — **no external assets**). One client at a time. All existing behavior,
validation, and security posture are preserved (FR-013); only presentation
changes, plus the new brightness endpoint.

## Shared shell

Every screen renders the same layout: a left **sidebar** (POV Display brand +
Overview / Settings nav, decorative profile footer) and a main content area, in
the dark theme by default. A small inline script provides the **Dark/Light theme
toggle** (sets `data-theme` on `<html>`, persisted in `localStorage`); it
requires no server interaction.

## GET / — Overview screen (re-skinned status page)

- Renders the **Overview** dashboard matching Figma frame `1:2`:
  - Metric cards: **Status** (connectivity state), **Network (SSID)**,
    **IP Address**, and **blink** state/frequency.
  - Optional action-notice banner (last Wi-Fi/firmware result).
  - Sidebar nav (Overview active) linking to `/settings`.
- Values reflect device state **as of page load** (no auto-refresh; FR-003a).
- MUST NOT display the stored password.

## GET /settings — Settings screen (Display + System + Network)

(Alias: `GET /wifi` returns the same screen for backward compatibility.)

Renders the **Settings** screen matching Figma frame `9:4`, with three cards:

- **Display card**:
  - Dark/Light **theme toggle** (client-side only).
  - **Brightness** control (range 0–100%), initialized to the current device
    brightness; submits to `POST /display`.
- **System card**:
  - **Firmware version** (read-only).
  - **Update Firmware** action (prominent/destructive style) → `GET /update`.
- **Network card**:
  - Current **SSID** + selectable nearby-network list (after a scan).
  - **Scan** action → `GET /settings?scan=1`.
  - **Password** field (masked, always empty on load).
  - Submit → `POST /config` (Wi-Fi change).
  - **Static IP** toggle + **IP / Subnet / Gateway** fields rendered
    **read-only** for fidelity (FR-018); not submittable.

## GET /settings?scan=1 — scan + list

- Triggers a Wi-Fi scan, waits within a bounded deadline, re-renders the Settings
  screen with the nearby-network list in the Network card (SSID + secured/open).
- Selecting a network fills the SSID field; manual entry remains available.
- No networks found → a clear in-card message; manual entry still works.
- Scanning MUST NOT permanently drop connectivity or corrupt stored credentials.

## POST /config — apply new Wi-Fi credentials (unchanged behavior)

- Body: form-encoded `ssid` and `password` (URL-decoded by the handler).
- No authorization required (open endpoint, consistent with `POST /update`).
- Behavior is identical to feature 006 (validate → stage + acknowledge with a
  re-skinned "applying" page → deferred test-connect in the runtime loop →
  persist on success / revert on failure → result banner). Only the rendered
  pages change.

## POST /display — set display brightness (NEW)

- Body: form-encoded `brightness` (integer 0–100).
- No authorization required (open endpoint, consistent with the other mutating
  portal endpoints).
- Behavior:
  1. **Parse & clamp** `brightness` into 0–100 (invalid/missing → rejected with
     the Settings screen re-rendered and an inline message; no change applied).
  2. **Apply**: update the runtime brightness so the next rendered LED frame uses
     it (via the driver brightness scalar).
  3. **Persist (on change only)**: if the new value differs from the stored one,
     write a V3 flash record preserving the current SSID/password/admin_token
     (atomic erase+write+verify). If unchanged, skip the flash write.
  4. **Respond**: re-render the Settings screen (or redirect to `/settings`) with
     the brightness control reflecting the applied value and a brief confirmation.
- The brightness value contains no secrets; logs may include the numeric value.

## GET /update, POST /update — firmware update (unchanged behavior)

- `GET /update`: re-skinned confirmation page (existing warning + 60 s countdown,
  Confirm/Cancel), reached from the System card's Update Firmware action.
- `POST /update`: re-skinned "rebooting" reply, then the device reboots into USB
  MSD (BOOTSEL) mode exactly as today.

## Behavioral Contracts

- **No external assets**: every screen renders fully offline (FR-012); no CDN
  fonts/images/scripts/styles.
- **Page budget**: each rendered page fits the static page buffer without
  truncation; an overflow returns an error response rather than a partial page.
- **Persistence**: applied credentials and the applied brightness are used
  automatically on the next boot.
- **Confidentiality**: the stored Wi-Fi password is never returned in any page.
- **Resilience**: a failed Wi-Fi change leaves the device reachable on its
  previous network (unchanged from 006).
- **Read-only fidelity controls**: Static-IP/IP/Subnet/Gateway and the
  account/notification/logout chrome perform no actions when interacted with.
- **LED timing**: applying brightness changes pixel values only; WS2812 bit
  timing and rotation/column timing are unaffected.

## Observability Contract

Required debug events (USB stdio), without printing the password:

- Overview/Settings pages served; scan started/finished with result count.
- Wi-Fi validation rejections (with reason); apply attempt + success/failure +
  revert (unchanged from 006).
- Brightness: received value, applied value (after clamp), and whether it was
  persisted (changed) or skipped (unchanged).
