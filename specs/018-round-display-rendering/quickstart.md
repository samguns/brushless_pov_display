# Quickstart: Validate Round-Display Cartesian Rendering

## Host unit checks

```powershell
g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test.exe
./build/pov_render_test.exe
```

Asserts the round-rendering contract: bounds safety (no write past active count),
disc masking (out-of-disc LEDs dark), color palette, blank-when-no-text, and
determinism.

## Visual reconstruction preview

```powershell
python tools/pov_preview.py tools/pov_target_123456.png             # ideal image
python tools/pov_preview.py --reconstruct tools/pov_recon_123456.png # 40-column sampling
```

Compare the ideal image with the 40-column reconstruction to confirm the digits
remain upright and legible at the device's actual angular resolution.

## Firmware build

```powershell
ninja -C build
```

## Hardware scenario

1. Calibrate time and spin at a supported speed.
2. Confirm the digits appear upright and horizontal (not radial arcs), centered in
   the disc, with hours red / minutes green / seconds blue.
