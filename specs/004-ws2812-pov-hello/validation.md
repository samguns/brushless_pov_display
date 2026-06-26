# Validation Log: WS2812 POV Hello Demo

## Build Validation

- Completed: `ninja -C build` succeeded (link step completed for `pov_leds.elf`).

## Static RAM Budget (Measured)

- Measured with `arm-none-eabi-gcc` (Cortex-M0+, `-fshort-enums`), matching the production toolchain.
- `static uint32_t frame_words[57]` = 228 B (only `static`-storage allocation).
- `ws2812_driver_t` = 52 B (`led_strip_config_t` 4 B + `output_health_state_t` 12 B + scalar fields 36 B).
- `pov_demo_t` = 24 B (`pov_demo_sequence_t` 10 B + `pov_playback_state_t` 12 B + alignment 2 B).
- Main-loop scratch state (timestamps + status flags) = 16 B.
- Feature RAM total: 320 bytes (< 512 B budget ceiling). No variance from spec budget.

## Scenario 1: Output-Path Readiness

- Pending hardware verification.

## Scenario 2/3: Hello Playback Timing and Looping

- Pending hardware verification.

## Scenario 4: Bounds and Fallback Behavior

- Pending hardware verification.

## SC-005 Startup Latency

- Pending hardware measurement from readiness to first `H`.

## SC-007 DMA Pipeline Evidence

- Software evidence: transfer path implemented in `ws2812_driver.cpp` via DMA -> TX FIFO -> PIO (`ws2812_driver_submit_frame` + DMA channel setup).
- Pending runtime capture on hardware.

## SC-008 Timing Source Evidence

- Software evidence: timing derives from `clock_get_hz(clk_sys)` in `ws2812_driver.cpp` (`ws2812_driver_init`).
- Pending runtime capture on hardware.

## Stability Soak

- Pending 5-minute hardware run at 57 LEDs.
