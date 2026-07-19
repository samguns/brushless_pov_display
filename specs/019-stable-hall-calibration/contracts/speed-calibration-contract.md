# Contract: Smoothed Rotation Speed Calibration

Applies to `pov_clock_rotation_update(rotation, measurement)`.

## Definitions

- `W = POV_CLOCK_SPEED_WINDOW` (8), `MIN = POV_CLOCK_SPEED_MIN_SAMPLES` (3).
- `mean` = running mean of accepted periods in the ring.
- A "new sample" is a measurement whose `sample_generation` differs from the stored
  one (re-reads of the same generation do not advance the filter).

## Guarantees

- **C1 (bounded smoothing)**: `rotation.period_us` equals the mean of at most the
  last `W` accepted revolution periods; history older than `W` samples has no
  effect.
- **C2 (phase preserved)**: `rotation.phase_reference_us == measurement.reference_edge_us`
  for every valid sample; the phase is never averaged.
- **C3 (outlier rejection)**: once `hist_count >= MIN`, a sample deviating from the
  mean by more than `POV_CLOCK_SPEED_OUTLIER_PCT` is not added to the ring and does
  not change `mean`, `rotation.period_us`, `rotation.rpm`, or `rotation.stable`.
- **C4 (responsiveness)**: after a sustained step to a new supported period, the
  mean reaches the new period after at most `W` accepted samples.
- **C5 (hysteresis)**: transitions use two thresholds — enter stable when the latest
  accepted sample is within `POV_CLOCK_SPEED_STABLE_ENTER_PCT` of the mean; leave
  stable only when a sample deviates by more than `POV_CLOCK_SPEED_STABLE_EXIT_PCT`.
- **C6 (confidence)**: while `hist_count < MIN`, `rotation.stable` is false and the
  status is not `SUITABLE` (existing startup fallback applies).
- **C7 (range from smoothed)**: `within_range` and `TOO_SLOW`/`TOO_FAST` are
  computed from the smoothed RPM.
- **C8 (stop handling)**: an invalid/stale measurement resets the ring and yields
  `UNAVAILABLE`; stop detection timing is unchanged.
- **C9 (generation dedup)**: re-reading the same `sample_generation` does not change
  the ring, mean, or stability.
- **C10 (no heap / bounded)**: all state is fixed-size; no allocation occurs.

## Non-goals

- Changing the supported RPM range, fallback meanings, or the LED transport.
- Smoothing the angular phase.
