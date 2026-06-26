# Quickstart & Validation: STA Management Portal & Firmware Update

**Date**: 2026-06-25
**Feature**: 002-sta-portal-fw-update

This guide validates the feature end-to-end on real hardware. There is no host-side unit
test framework for this RP2040 firmware; validation is build-time (linker assertion) plus
on-device manual checks using USB serial output.

---

## Prerequisites

- Raspberry Pi Pico W flashed via USB.
- Pico SDK 2.2.0 + ARM toolchain `14_2_Rel1` (already configured under `~/.pico-sdk`).
- A 2.4 GHz WiFi network and its password.
- A host computer with a USB-serial terminal (USB stdio is enabled in `CMakeLists.txt`).
- The device already provisioned at least once with feature 001 (so credentials exist), or
  ready to be provisioned via the AP portal on first boot.

---

## Build

```bash
ninja -C build
```

Expected: a clean build producing `build/pov_leds.uf2`.

**Build-time validation (FR-003 / SC-005)** — confirm the credential region is protected by
the linker. Temporarily verify the assertion fires by checking the map file boundary:

```bash
# Firmware end must be below the reserved sector start (0x101FF000)
grep -m1 "__flash_binary_end" build/pov_leds.elf.map
```

Expected: `__flash_binary_end` is well below `0x101FF000`. (To prove the guard works, an
artificial oversize build must fail linking with the message
`ERROR: firmware overflows reserved WiFi credential sector` — not produce a UF2.)

---

## Flash

```bash
picotool load build/pov_leds.uf2 -fx
```

Or use the VS Code "Run Project" task.

---

## Scenario 1 — Credentials survive a firmware update (US1)

1. Provision the device (feature 001 AP flow) so credentials are stored.
2. Record the credential region before update:
   ```bash
   picotool save -r 0x101FF000 0x10200000 before.bin
   ```
3. Rebuild and reflash a new `pov_leds.uf2` (e.g. bump `pico_set_program_version`).
4. After reboot, read the region again:
   ```bash
   picotool save -r 0x101FF000 0x10200000 after.bin
   diff before.bin after.bin && echo "UNCHANGED (PASS)"
   ```

**Expected**: `before.bin` and `after.bin` are identical (SC-006). On the serial console the
device connects in STA mode within 20 s **without** starting the AP (FR-001, FR-002).

---

## Scenario 2 — STA management page shows IP and SSID (US2)

1. Boot a provisioned device. Open the USB serial terminal.
2. Observe the IP/SSID line, e.g.:
   ```
   [wifi_config] STA connected — SSID: MyHomeWiFi  IP: 192.168.1.42
   ```
   (Printed within 500 ms of the DHCP lease — FR-005, SC-002.)
3. On a computer on the same LAN, browse to `http://192.168.1.42/`.

**Expected**: The management page loads within 3 s (SC-003) and clearly shows the device IP
and the connected SSID (FR-006, FR-007), with an **"Update firmware"** button (FR-008).

---

## Scenario 3 — Trigger firmware update mode (US3)

1. From the management page, click **"Update firmware"**.
2. **Expected**: A confirmation page appears with **Confirm update**, **Cancel**, and a
   visible **60 s countdown** (FR-009).
3. Click **Cancel**.
   - **Expected**: Returns to the status page; device stays connected, no disruption
     (FR-011).
4. Click **Update firmware** again, then **Confirm update**.
   - **Expected**: A "Rebooting into update mode…" page is shown, then within 3 s the host
     mounts the `RPI-RP2` USB drive (FR-010, SC-004).
5. Drag a new `pov_leds.uf2` onto the drive.
   - **Expected**: The device reflashes, reboots, and auto-reconnects in STA mode using the
     preserved credentials.

---

## Scenario 4 — WiFi drop fallback (FR-014)

1. While the management page is reachable, power off the router (or move the device out of
   range).
2. **Expected (serial)**: The device attempts a silent reconnect. If it cannot reconnect
   within 15 s, it falls back to AP provisioning mode (`pov-leds-setup` appears).

---

## References

- Endpoints and page contracts: [contracts/http-api.md](contracts/http-api.md)
- Flash layout & RAM budget: [data-model.md](data-model.md)
- Design decisions: [research.md](research.md)
