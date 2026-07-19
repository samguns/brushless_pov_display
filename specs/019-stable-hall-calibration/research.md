# Research: Stable Hall Speed Calibration

## Current behavior

- `hall_sensor_derive` computes `period_us` from the single most recent interval
  (`last_interval_us`). Any per-edge timing noise directly perturbs the period.
- `pov_clock_rotation_update` compares the current period to the previous period
  and flags `UNSTABLE` if they differ by more than 15% (single-step, no smoothing,
  no hysteresis → can flap).
- The renderer uses `rotation.period_us` for column spacing and
  `rotation.phase_reference_us` (the real Hall edge) for angular phase.

## Decision: bounded moving average + outlier rejection + hysteresis

Add state to `pov_clock_rotation_t` and logic to `pov_clock_rotation_update`,
applied only on a new Hall sample generation:

1. **Sample** = `measurement.period_us` (already bounded to the supported window).
2. **Outlier rejection**: once at least `kMinSamples` are collected, if the sample
   deviates from the current mean by more than `kOutlierPercent` (40%), reject it
   (do not add to the ring, do not change the mean or stability). This covers a
   missed reference (~2x) and a bounce (~0.5x).
3. **Bounded moving average**: push accepted samples into a fixed ring of
   `kSmoothWindow` (8) entries; keep a running sum; `smoothed = sum / count`.
4. **Output**: set `rotation.period_us = smoothed`, `rotation.rpm = 60e6 /
   smoothed`. Keep `rotation.phase_reference_us = measurement.reference_edge_us`
   (the real edge) so phase does not drift.
5. **Hysteresis**: maintain `stable`. When unstable, become stable only if the
   latest accepted sample is within `kStableEnterPercent` (10%) of the mean and
   `count >= kMinSamples`. When stable, become unstable only if a sample deviates
   more than `kStableExitPercent` (25%). Rejected outliers never flip stability.
6. **Confidence**: before `kMinSamples`, `stable = false` (existing startup
   fallback applies, FR-007).

### Why a bounded window (not a lifetime mean)

- A lifetime cumulative mean never forgets and lags real speed changes
  indefinitely — it would show a wrong image after a spin-up until the average
  slowly catches up. A window of 8 converges within ~8 revolutions (FR-002, FR-005).
- Moving average of N=8 reduces standard deviation by ~sqrt(8) ≈ 2.8x (~65%),
  meeting SC-001 (>=60%).
- EMA is an alternative with similar behavior; a ring moving average was chosen for
  deterministic, exactly-bounded memory and simple host-test reasoning.

### Why phase stays on the real edge

Smoothing the period improves cadence stability, but the once-per-rev angular zero
must remain the physical edge; averaging the phase would introduce drift. This
preserves feature 015's phase-lock guarantee.

## Alternatives considered

- **Unbounded cumulative mean** (user's initial phrasing): simplest but lags real
  changes and never forgets. Rejected per FR-002.
- **Median-of-N**: excellent outlier rejection but costs a sort each sample and
  more state; the mean + explicit outlier band achieves the goal more cheaply.
- **Separate `hall_speed_filter` module**: cleaner separation but adds a source
  file and CMake wiring; integrating into `pov_clock_rotation_update` keeps the
  existing one-sample-per-generation contract and host test setup.

## Testability

`pov_clock_rotation_update` is a pure function of the measurement stream and its
own state. Host tests feed synthetic measurements to assert: variance reduction,
convergence within a window, single-outlier tolerance, hysteresis (no flapping),
min-sample confidence, and that `phase_reference_us` equals the fed edge.
