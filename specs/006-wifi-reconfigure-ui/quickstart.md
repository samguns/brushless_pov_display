# Quickstart: Wi-Fi Reconfiguration UI

**Feature**: 006-wifi-reconfigure-ui

Validates changing the device's Wi-Fi network from the STA management UI. See
[contracts/http-api.md](contracts/http-api.md) and [data-model.md](data-model.md)
for endpoint and field details.

## Prerequisites

- Pimoroni Pico Plus 2 W with the project firmware, already provisioned and
  connected to an initial network (STA mode).
- A computer/phone that can reach the device's management page on the current
  network.
- A second WPA2 network (different SSID) reachable by the device, with its
  password, for the change test.

## Build & Flash

```bash
ninja -C build
picotool load build/pov_leds.uf2 -fx
```

## Scenario 1 — Discover the reconfiguration UI (US1/US3)

1. Open the device status page (`http://<device-ip>/`).
2. Confirm a link/button to change Wi-Fi is present and open it (`/wifi`).
3. **Expected**: the current SSID is shown, the password field is masked/empty,
   and manual SSID + password fields with a "Scan" button are present.

## Scenario 2 — Change network via manual entry (US1)

1. Enter the second network's SSID and password; submit.
2. **Expected**: an "applying" page appears telling you the link will drop and to
   reconnect to the new network. The device then joins the new network.
3. Reconnect your client to the new network and open the device page.
4. **Expected**: the status/Change-Wi-Fi page shows a success banner and the new
   SSID; the device is reachable there.
5. Power-cycle the device.
6. **Expected**: it reconnects automatically to the new network (SC-002).

## Scenario 3 — Pick from a scanned list (US4)

1. On `/wifi`, press "Scan".
2. **Expected**: nearby networks are listed with a secured/open indication;
   selecting one fills the SSID field so only the password is needed (SC-007).
3. Complete a change using a selected network.
4. **Expected**: success as in Scenario 2; the live link may briefly blip during
   the scan but recovers (FR-017).

## Scenario 4 — Safe failure on bad credentials (US2)

1. Submit a valid SSID with a wrong password.
2. **Expected**: an "applying" page appears; the connect fails and the device
   reverts to its previous network and remains reachable there (SC-003).
3. Reconnect to the previous network and open the device page.
4. **Expected**: a failure banner indicates the change did not take effect.
5. Power-cycle the device.
6. **Expected**: it boots on the previous (working) network, not the rejected one.

## Scenario 5 — Input validation (US3)

1. Submit an empty SSID, then an over-length SSID/password.
2. **Expected**: each is rejected with a clear message and no disconnect/reconnect
   occurs (SC-005).

## Confidentiality check (SC-006)

- Review the status and reconfiguration pages and `/status` output; confirm the
  stored password is never shown in plaintext.

## Notes

- Hidden (non-broadcast) networks won't appear in the scan list; use manual entry.
- Reconfiguration briefly pauses normal operation during the bounded scan/connect
  windows; this is expected for this infrequent action.
