# Validation Log: Round-Display Cartesian Text Rendering

## Automated verification

- Renderer host test: PASS. `test_round_rendering` covers blank-when-no-text,
  per-column bounds safety with guard words past `frame_len`, palette-color
  validity, non-empty text, column-0 legibility, determinism, and reduced-count
  safety. Command:
  `g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test.exe`
  → "adaptive Hall rendering tests passed".
- Firmware build `ninja -C build`: PASS (recompiled renderer + relinked
  `pov_leds.elf`).
- Visual reconstruction preview: PASS. `tools/pov_preview.py --reconstruct`
  renders the 40-column polar sampling; central digits are well-formed, outer
  digits are coarser (expected at large radius under the 40-column cap).

## Fixed-memory impact

- Adds file-static 57x57 palette framebuffer (3249 bytes) + two int16[40] trig
  tables (160 bytes); removes per-renderer `column_masks[40]` (40 B) and
  `column_colors[40]` (160 B).
- Measured firmware totals after change: text 402,896 bytes, data 44 bytes,
  BSS 71,912 bytes (BSS +~3.4 KB vs prior, matching the framebuffer + trig tables).
- No heap allocation; framebuffer/trig are file-static (not on the main stack).

## Hardware validation

- Upright, centered, correctly colored digits on the disc: PENDING (on-blade).
- Angular-resolution coarseness at large radius: expected, not a defect.

## Residual risks

- Geometry assumption (57 LEDs span the diameter, CENTER=28). If the arm is a
  radius, adjust CENTER and radius sign.
- 40-column cap limits rim sharpness; increasing it requires faster LED transport
  or fewer LEDs.
