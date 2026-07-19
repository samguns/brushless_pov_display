# Quickstart: Validate Full 57-LED POV Coverage

## Host unit checks

Compile and run the renderer host test directly (no hardware required):

```powershell
g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test.exe
./build/pov_render_test.exe
```

The test asserts the full-coverage contract:

1. For a column with the top and bottom glyph rows set, LED 0 and LED
   `active_count - 1` are both lit (C1).
2. Every LED index in the active span maps to a glyph row — no reserved dark
   margin exists (C2).
3. No word is written at or beyond `active_led_count` (a canary word after the
   active region stays zero) (C3).
4. A blank column (mask 0) produces an all-dark frame (C4).
5. The mapping holds for the full count (57) and for a reduced active count (C3/US3).

## Firmware build

```powershell
ninja -C build
```

Confirm the firmware links with no size regression beyond the expected (none, since
no buffers are added).

## Hardware scenario: full-height image

1. Calibrate time and spin the blade at a supported speed.
2. Observe the clock image and confirm lit pixels reach both the innermost and
   outermost LED rows — no dark band at the inner or outer edge.
3. Confirm the digits are visibly taller than before and remain the correct digits.

## Hardware scenario: status coverage

1. Force a non-normal state (e.g. before time calibration).
2. Confirm the status indicator pattern spans the full LED height rather than a
   centered subset.
