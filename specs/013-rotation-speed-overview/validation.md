# Validation Log: Rotation Speed Overview

## Automated verification

- ninja -C build: PASS. All changed C/C++ translation units compiled and
  pov_leds.elf linked successfully.
- Host page-builder check: PASS. Unavailable status rendered -- RPM; available
  status rendered 600 RPM; the complete test page was 8,743 bytes, within the
  existing 16,384-byte buffer.
- git diff --check: PASS (line-ending conversion notices only).

## Scenario evidence

- Startup/unavailable: rendering verified in the host builder; hardware/browser
  confirmation remains pending.
- Valid rotation: integer rendering verified in the host builder; comparison to
  physical reference RPM remains pending.
- Stopped rotation: code path publishes zero after prior validity and Hall
  staleness; hardware/browser timeout confirmation remains pending.
- Responsive layout: existing flex-wrap metric-card layout is unchanged; visual
  device-browser confirmation remains pending.

## Static RAM impact

- wifi_runtime_state_t grew by 8 bytes for aligned availability/RPM fields.
- HTTP status adds a 1-byte availability symbol and 4-byte RPM symbol, confirmed
  in the linked ELF with arm-none-eabi-nm -S.
- Named persistent static-state increase: 13 bytes (linker placement/alignment
  may consume padding around symbols). Main also retains one boolean and one
  32-bit RPM local on its existing fixed stack; no heap or new buffer is used.
