# Quickstart: Validate Stable Hall Speed Calibration

## Host unit checks

```powershell
g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test.exe
./build/pov_render_test.exe
```

Assertions cover:

1. Variance reduction: a noisy constant-speed stream yields a smoothed
   `period_us` whose spread is far smaller than the raw per-sample spread.
2. Convergence: after a sustained step to a new supported speed, the smoothed
   period reaches the new value within the window.
3. Outlier rejection: one very short and one very long interval barely move the
   estimate and do not flip stability.
4. Hysteresis: a near-threshold sequence does not toggle stable/unstable.
5. Confidence: before the minimum sample count, status is not `SUITABLE`.
6. Phase preserved: `phase_reference_us` equals the fed edge each sample.
7. Generation dedup and stop/reset behavior unchanged.

## Firmware build

```powershell
ninja -C build
```

## Hardware scenario

1. Spin at a steady supported speed; confirm the image is visibly steadier than
   before (no per-revolution shimmer).
2. Step the speed to another supported value; confirm the image re-stabilizes
   within a few revolutions.
3. Introduce a brief disturbance (or rely on natural glitches); confirm a single
   glitch does not cause a visible jump or a fallback flash.
