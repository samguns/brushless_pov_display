# Validation Log: Clock and RPM Overview

## Automated verification

- ninja -C build: PASS. All changed C/C++ units compiled and pov_leds.elf linked.
- Host page-builder matrix: PASS for both unavailable, calibrated clock only,
  calibrated clock plus 600 RPM, and midnight plus stopped 0 RPM.
- The largest checked Overview response was 8,834 bytes, below the existing
  16,384-byte fixed page buffer.
- git diff --check: PASS (line-ending conversion notices only).

## Scenario evidence

- Clock unavailable / RPM unavailable: host rendering verified.
- Clock calibrated / RPM unavailable: host rendering verified.
- Clock calibrated / RPM valid: host rendering verified with 23:59:59 CST and
  600 RPM.
- Midnight is distinct from unavailable: host rendering verified with
  00:00:00 CST.
- Clock advancing / RPM stopped: independent 00:00:00 CST and 0 RPM rendering
  verified; physical timeout and advancing-clock behavior remain pending on hardware.
- Responsive layout: existing flex-wrap card layout is unchanged; device-browser
  visual confirmation remains pending.

## Static RAM impact

- wifi_runtime_state_t increased from 208 to 216 bytes: 8 bytes after structure
  layout reused existing padding.
- HTTP-owned symbols add 1 byte for availability and 9 bytes for clock text.
- Named persistent static-state increase is 18 bytes. No heap, frame buffer, page
  buffer, interrupt state, or LED-output allocation was added.
