# Data Model: Stable Hall Speed Calibration

## Changed entity: `pov_clock_rotation_t`

Existing fields (unchanged): `rpm`, `period_us`, `phase_reference_us`,
`sample_generation`, `fresh`, `within_range`, `stable`, `status`,
`previous_period_us`.

Added fields (fixed-size, zero-initialized by `memset` in
`pov_clock_rotation_init`):

- `uint32_t period_hist[POV_CLOCK_SPEED_WINDOW]` — ring of recent accepted periods.
- `uint64_t period_sum` — running sum of the ring for O(1) mean.
- `uint8_t hist_count` — number of valid entries (0..window).
- `uint8_t hist_head` — next write index into the ring.
- `uint32_t smoothed_period_us` — current bounded-mean period (mirror of `period_us`).

## New constants (`pov_clock.h`)

- `POV_CLOCK_SPEED_WINDOW = 8` — moving-average window (revolutions).
- `POV_CLOCK_SPEED_MIN_SAMPLES = 3` — samples before a confident speed.
- `POV_CLOCK_SPEED_OUTLIER_PCT = 40` — reject band vs current mean.
- `POV_CLOCK_SPEED_STABLE_ENTER_PCT = 10` — enter-stable band.
- `POV_CLOCK_SPEED_STABLE_EXIT_PCT = 25` — exit-stable band (hysteresis).

## Semantics

- **Sample source**: `measurement.period_us` per new `sample_generation`.
- **Mean**: `smoothed_period_us = period_sum / hist_count`.
- **Rendering outputs**: `period_us = smoothed_period_us`, `rpm = 60e6 /
  smoothed_period_us`; `phase_reference_us = measurement.reference_edge_us`.
- **Validity**: `within_range`/`status` computed from the smoothed RPM.
- **Stopped/invalid**: on stale/invalid measurement, reset ring
  (`hist_count = 0`, `period_sum = 0`) and report `UNAVAILABLE`, as today.

## Relationships

```
measurement (per generation) ──▶ outlier policy ──▶ ring buffer ──▶ smoothed_period
smoothed_period ──▶ rotation.period_us / rotation.rpm ──▶ renderer cadence
measurement.reference_edge_us ──▶ rotation.phase_reference_us ──▶ renderer phase
smoothed vs latest sample + hysteresis ──▶ rotation.stable ──▶ status
```
