# Quickstart & Validation: Management Portal UI/UX Redesign

**Feature**: 007-portal-ui-redesign

This guide validates that the redesigned portal matches the Figma design and that
the new brightness/theme controls work, without changing existing behavior.

References: [plan.md](plan.md) · [contracts/http-api.md](contracts/http-api.md) ·
[data-model.md](data-model.md) · Figma Overview `1:2`, Settings `9:4`.

## Prerequisites

- Pimoroni Pico Plus 2 W with the WS2812 panel attached (as today).
- The device already provisioned with Wi-Fi credentials (STA mode) on a network
  your test browser can also join.
- A desktop browser and a phone browser on the same LAN.
- Toolchain per the constitution (Pico SDK 2.2.0); USB stdio on for logs.

## Build & flash

```bash
ninja -C build
picotool load build/pov_leds.uf2 -fx
```

Single-command build must succeed from a cold clone (Principle V). Confirm the
new sources compile and the `wifi_flash_record_v3_t` static-size assert passes.

## Find the device

Open the device IP (from the serial log `IP=...`, or your router) in a browser:
`http://<device-ip>/`.

## Scenario A — Overview screen (User Story 1)

1. Load `http://<device-ip>/`.
2. **Expect**: a dark layout with a left sidebar ("POV Display", Overview active,
   Settings link) and metric cards for **Status**, **Network (SSID)**,
   **IP Address**, and **blink** — values matching the device's real state.
3. Compare side-by-side with Figma frame `1:2`: layout, cards, colors, and
   monospaced values should match (SC-001, SC-011).
4. If a recent Wi-Fi/firmware action occurred, its notice shows as a styled
   banner.

## Scenario B — Settings layout (User Stories 2, 3, 5)

1. Click **Settings** in the sidebar → `GET /settings`.
2. **Expect** three cards matching Figma frame `9:4`:
   - **Display**: theme toggle + brightness control.
   - **System**: firmware version + red/prominent **Update Firmware**.
   - **Network**: SSID + Scan, password, and read-only Static IP / IP / Subnet /
     Gateway.

## Scenario C — Theme toggle (User Story 5 / SC-008)

1. On any screen, toggle **Light**.
2. **Expect**: the whole portal restyles to light instantly; reload — the choice
   persists (browser `localStorage`). Toggle back to **Dark** (the default on a
   fresh browser).

## Scenario D — Brightness controls the LEDs and persists (User Story 5 / SC-009)

1. On Settings, set Brightness to a low value (e.g. 20%) and submit.
2. **Expect**: the WS2812 panel visibly dims; serial log shows the received and
   applied value and that it was persisted (changed).
3. Set it back to a higher value (e.g. 90%) and submit → panel brightens.
4. Power-cycle the device. After boot, **expect** the panel to come up at the
   last set brightness (90%), and Settings to show ~90% (persistence via V3
   flash record).
5. Submit the same value again → serial log shows the flash write was **skipped**
   (unchanged), confirming write-on-change.

## Scenario E — Wi-Fi change still works (User Story 2 / SC-003)

1. On Settings → Network, click **Scan**; **expect** nearby networks listed
   (or a clear "no networks" message). Select one to fill the SSID.
2. Enter a valid password (8–63 chars) and submit.
3. **Expect** the re-skinned "applying" page; the device switches networks and,
   on success, persists and resumes on the new network (revert on failure).
   Behavior is unchanged from feature 006 — only the styling differs.
4. Try an invalid password (e.g. 3 chars): **expect** an inline validation
   message and no network change.

## Scenario F — Firmware update reachable (User Story 3 / SC-004)

1. On Settings → System, click **Update Firmware**.
2. **Expect** the re-skinned confirmation page (warning + 60 s countdown,
   Confirm/Cancel). Confirm → device reboots into USB MSD mode as today. (Cancel
   to avoid rebooting during UI validation.)

## Scenario G — Read-only fidelity controls (SC-010)

1. On Settings → Network, confirm the Static IP toggle is off and IP / Subnet /
   Gateway are visible but **not editable / not submittable**.
2. Confirm the sidebar profile, header avatar, notification, and logout elements
   are present but inert (no navigation/state change).

## Scenario H — Responsive / offline (SC-005, SC-006)

1. Open the portal on a phone (or narrow the desktop window to ≤ 420 px).
2. **Expect**: cards/sidebar reflow, content readable, all actions reachable with
   **no horizontal scrolling**.
3. Confirm pages fully render with the device offline from the internet (only the
   LAN link to the device is needed) — no external requests (SC-006).

## Page-budget check (FR-014 / SC-007)

- For the largest page (Settings with a full 20-network scan list), inspect the
  response `Content-Length` (browser dev tools or `curl -sI`) and confirm it is
  comfortably below `STA_PAGE_BUF_SIZE` (16384). No page should be truncated.

## Regression sweep

- Status/Overview values correct; `GET /status` JSON (if used) still valid.
- `GET /wifi` alias still returns the Settings screen.
- No heap added; build clean; existing demo/LED output unaffected aside from the
  intended global brightness scaling.
