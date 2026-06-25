# Quickstart Validation Guide: WiFi Configuration Web App

**Date**: 2026-06-25
**Feature**: 001-wifi-config-webapp

This guide describes how to validate the feature end-to-end after implementation. It is a validation/run guide, not an implementation guide.

---

## Prerequisites

- Raspberry Pi Pico W flashed with the `pov_leds` firmware (built with this feature enabled).
- A WiFi access point within range that you know the SSID and password for.
- A second device (phone or laptop) with WiFi capability and a web browser.
- A USB cable for flashing (or picotool if already loaded).
- (Optional) A USB serial terminal (e.g., `minicom`, `screen`, PuTTY) to observe debug output if stdio UART is enabled.

---

## Build

```sh
ninja -C build
```

Produces `build/pov_leds.uf2`.

---

## Flash

```sh
~/.pico-sdk/picotool/2.2.0-a4/picotool/picotool load build/pov_leds.uf2 -fx
```

Or drag-and-drop `build/pov_leds.uf2` onto the Pico W mass-storage device (hold BOOTSEL while plugging in).

---

## Scenario 1: First-Time Setup (No Credentials Stored)

**Validates**: FR-001, FR-003, FR-005, FR-006, FR-006b, FR-006c, FR-007, FR-008, FR-011, FR-013, SC-001, SC-002, SC-003, SC-006

### Steps

1. Flash the device (or erase flash to clear any stored credentials: `picotool erase`).
2. Power on the device.
3. On your phone/laptop, open WiFi settings. Within **10 seconds**, a network named `pov-leds-setup` should appear. *(Validates SC-002)*
4. Connect to `pov-leds-setup` using password `12345678`.
5. Your OS should display a "Sign in to network" prompt or open a browser automatically (captive portal). If not, open a browser and navigate to `http://192.168.4.1`. *(Validates captive portal edge case)*
6. The configuration page loads. *(Validates FR-005, SC-006)*
7. Verify a list of nearby WiFi networks is displayed, sorted by signal strength. *(Validates FR-013)*
8. Select your target network from the list.
9. Enter the correct password.
10. Tap / click "Connect".
11. The browser shows the "Connecting…" page. *(Validates FR-007)*
12. Within **30 seconds**, the `pov-leds-setup` AP disappears from your WiFi list. *(Indicates success — Validates SC-003)*
13. Reconnect your phone/laptop to your home WiFi.

**Expected outcome**: `pov-leds-setup` is no longer visible. The device is connected to your home network. Credentials are saved.

---

## Scenario 2: Automatic Connection on Subsequent Boot

**Validates**: FR-001, FR-002, SC-004

### Steps

1. After Scenario 1, reboot the device (unplug and replug USB power).
2. On your phone/laptop, scan for WiFi networks.
3. `pov-leds-setup` must NOT appear. *(Validates FR-002)*
4. Within **20 seconds** of power-on, the device should be reachable on your home network. *(Validates SC-004)*

**Expected outcome**: No AP is broadcast. Device is online silently.

---

## Scenario 3: Wrong Password Recovery

**Validates**: FR-004, FR-009, FR-010, SC-005

### Steps

1. Connect to `pov-leds-setup` (AP must be active — either first boot or after clearing credentials).
2. Open the configuration page.
3. Select your target network.
4. Enter a deliberately incorrect password (e.g., `wrongpassword`).
5. Submit.
6. The "Connecting…" page is shown.
7. Within **30 seconds**, `pov-leds-setup` reappears. *(Device fell back to AP on failed connection)*
8. Reconnect to `pov-leds-setup`.
9. Open `http://192.168.4.1` in a browser.

**Expected outcome**:
- An error message is displayed (e.g., "Incorrect password"). *(Validates FR-009)*
- The form is available to try again without rebooting. *(Validates FR-010)*
- No credentials were saved (verify by rebooting: AP reappears again). *(Validates SC-005)*

---

## Scenario 4: Hidden Network (Manual SSID Entry)

**Validates**: FR-006b

### Steps

1. In the configuration page network list, scroll to the bottom and select "Join other network…".
2. A text field appears for manual SSID entry.
3. Type the SSID of a hidden network (one not shown in the list).
4. Enter its password.
5. Submit and follow the same outcome as Scenario 1.

**Expected outcome**: Device connects to the hidden network successfully.

---

## Scenario 5: Empty SSID Rejected

**Validates**: FR-006c, contracts/http-api.md validation rules

### Steps

1. On the configuration page, select "Join other network…" but leave the manual SSID field empty.
2. Submit the form.

**Expected outcome**: An error response is returned (`400`). No connection attempt is made. AP remains up.

---

## Scenario 6: Credential Replacement

**Validates**: FR-012

### Steps

1. Complete Scenario 1 with Network A credentials.
2. Trigger AP mode again (erase credentials or use a future "reset" mechanism, or store wrong credentials to trigger fallback).
3. In the configuration page, select and submit credentials for Network B.
4. Reboot.

**Expected outcome**: Device connects to Network B, not Network A. Old credentials are overwritten.

---

## References

- Data model (flash layout, credential struct): [data-model.md](data-model.md)
- HTTP interface (endpoints, request/response format): [contracts/http-api.md](contracts/http-api.md)
- Research (AP↔STA constraint, flash write mechanics): [research.md](research.md)
- Functional requirements: [spec.md](spec.md)
