# Data Model: STA POV Clock Display

## Time Sync State

Represents the network calibration workflow.

**Fields**:

- `state`: idle, resolving, sending, waiting, calibrated, failed.
- `server_name`: configured network time host.
- `request_started_ms`: wall-clock attempt start time.
- `last_attempt_ms`: most recent retry timestamp.
- `resolved_addr`: resolved IP address when DNS succeeds.
- `calibrated_utc_seconds`: UTC epoch seconds from the accepted response.
- `calibrated_at_us`: monotonic microsecond timestamp when calibration was accepted.
- `attempt_count`: bounded retry counter.
- `last_error`: none, DNS failed, timeout, malformed response, transport failure.

**Validation Rules**:

- A calibration is valid only after a syntactically valid network time response.
- Calibration attempts must time out within the feature's 10-second success
  target on a reachable network.
- The state machine must be stepped non-blockingly from the super-loop.

## Calibrated Time State

Represents the usable wall-clock basis.

**Fields**:

- `calibrated`: true when UTC time is available.
- `utc_seconds_base`: UTC epoch seconds at calibration.
- `base_monotonic_us`: local monotonic timestamp at calibration.
- `current_utc_seconds`: derived current UTC epoch seconds.
- `current_cst`: derived China Standard Time value.
- `age_seconds`: elapsed seconds since calibration.

**Relationships**:

- Updated from Time Sync State after calibration.
- Read by the renderer through a CST Clock Value.

**Validation Rules**:

- `current_utc_seconds` advances from monotonic elapsed time.
- CST conversion applies exactly +8 hours and no daylight-saving adjustment.
- Uncalibrated state must not produce a normal clock display.

## CST Clock Value

Represents the user-visible time text.

**Fields**:

- `hour`: 0-23.
- `minute`: 0-59.
- `second`: 0-59.
- `timezone_label`: fixed `CST`.
- `text`: compact display string `HH:MM:SS CST`.

**Validation Rules**:

- Time fields must wrap correctly at minute, hour, and local day boundaries.
- The displayed second changes once per wall-clock second.

## Rotation Suitability State

Represents whether measured rotation can support POV clock rendering.

**Fields**:

- `rpm`: latest measured revolutions per minute.
- `period_us`: latest measured revolution period.
- `fresh`: true when the Hall reading is valid and not stale.
- `within_range`: true from 480 RPM through 800 RPM inclusive.
- `stable`: true when recent period variation stays within the accepted jitter
  threshold selected during implementation.
- `status`: unavailable, too_slow, suitable, too_fast, unstable.

**Relationships**:

- Derived from `hall_rotation_measurement_t`.
- Consumed by Display Health State and the renderer scheduler.

**Validation Rules**:

- Stale Hall readings are never suitable.
- The nominal target is 600 RPM.
- Out-of-range speeds must leave normal clock display.

## POV Clock Renderer State

Represents the compact 40-column clock layout and current column scheduling.

**Fields**:

- `column_count`: 48.
- `active_column`: current angular column index.
- `last_column_us`: monotonic timestamp of the last emitted column.
- `column_interval_us`: derived from measured rotation period / 48.
- `last_rendered_second`: second value represented by the current text layout.
- `text`: latest `HH:MM:SS CST` string.

**Relationships**:

- Reads CST Clock Value once per second or when entering normal mode.
- Reads Rotation Suitability State to derive column timing.
- Produces 57-word LED frames for `ws2812_driver_submit_frame`.

**Validation Rules**:

- Column interval must be recomputed from measured period when the period changes.
- Renderer must not write beyond 57 LED frame words.
- Renderer state must be bounded and heap-free.

## Display Health State

Represents the current externally observable display mode.

**States**:

- `time_unavailable`
- `rotation_unavailable`
- `speed_too_slow`
- `speed_too_fast`
- `speed_unstable`
- `normal_clock`

**Transitions**:

- `time_unavailable` -> `normal_clock` when time is calibrated and rotation is
  suitable.
- `normal_clock` -> `time_unavailable` if calibration is lost or invalidated.
- `normal_clock` -> `rotation_unavailable` if the Hall reading becomes stale.
- `normal_clock` -> `speed_too_slow` or `speed_too_fast` when RPM leaves range.
- Any invalid state -> `normal_clock` when time and rotation return to valid.

**Validation Rules**:

- Normal clock is allowed only when calibrated time and suitable rotation are
  both true.
- Invalid conditions must leave normal clock mode within 2 revolutions or 1
  second, whichever occurs first.
