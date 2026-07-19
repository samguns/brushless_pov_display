# Validation Log: Stable Hall Speed Calibration

## Automated verification

- Rotation host test: PASS. `test_speed_calibration` covers confidence gating,
  phase preservation, generation dedup, ~variance reduction (alternating ±2%),
  outlier rejection (2x/0.5x), hysteresis (~18% no-destabilize), convergence within
  the window after a step, and stop/reset. `test_supported_envelope` updated to feed
  the confidence threshold. Command:
  `g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test.exe`
  → "adaptive Hall rendering tests passed".
- Firmware build `ninja -C build`: PASS (recompiled `pov_clock.cpp` + relinked).

## Fixed-memory impact

- Adds `uint32_t period_hist[8]` (32 B) + `uint64_t period_sum` (8 B) + `hist_count`
  + `hist_head` + `uint32_t smoothed_period_us` to `pov_clock_rotation_t`. This
  struct is a stack local in `main` (not static/BSS), so BSS is unchanged
  (71,912 bytes); firmware text ≈ 402,880 bytes. No heap; removed the unused
  `kInstabilityPercent` constant.

## Hardware validation

- Steadier image at constant speed vs prior: PENDING (on-blade).
- Re-stabilization within a few revolutions after a speed step: PENDING.
- Single-glitch immunity (no fallback flash): PENDING.

## Residual risks

- Window size / thresholds are planning defaults; hardware tuning may adjust them.
- Very rapid real speed changes faster than the window will lag by design; the
  window balances noise rejection against responsiveness.
