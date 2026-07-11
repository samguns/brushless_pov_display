# Contract: Adaptive Hall-Synchronized Rendering

## Hall measurement contract

A fresh valid measurement provides RPM, revolution period, and the latest
accepted Hall edge timestamp. The timestamp and period share the same monotonic
microsecond timebase. A sample generation changes once per accepted Hall edge.
Invalid/stale measurements do not authorize normal output.

## Rotation eligibility contract

- 480 RPM and 800 RPM are suitable boundary values.
- Fresh stable values between them are suitable regardless of distance from the
  600 RPM reference.
- Values below/above the envelope map to too-slow/too-fast states.
- Stale or incomplete measurement maps to unavailable.
- Suitable state retains the measured period and Hall phase reference.
- Period-history stability changes only for a new Hall sample generation.

## Renderer contract

Input:

- bounded revolution period,
- Hall phase-reference timestamp,
- current monotonic timestamp.

Behavior:

- Maps absolute phase to one of 40 angular columns.
- Returns a render decision when the current target differs from the last output
  or when a new phase lock must be established.
- Skips expired columns after delay; never replays them.
- Re-anchors on fresh Hall reference timestamps.
- Rejects zero period, future/invalid phase reference, or unavailable state.
- Uses no heap and performs bounded constant-time arithmetic.

## LED transport contract

- A selected radial frame is submitted only when the WS2812 driver is ready and
  its DMA channel, PIO wire time, and latch interval are complete.
- Busy transfer means the current column is dropped, not queued or replayed.
- The deterministic wire-plus-latch duration is included when selecting the
  column expected at presentation time.
- PIO bit timing, brightness scaling, and LED color packing remain unchanged.

## Observable states

- Calibrated time plus suitable phase-aware rotation selects normal clock output.
- Uncalibrated time, stale rotation, too-slow, too-fast, and unstable rotation
  retain their existing fallback meanings.
