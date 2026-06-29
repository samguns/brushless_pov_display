# Contract: Hall Sensor Rotation Speed Driver

**Feature**: 005-hall-sensor-speed

## Scope

Defines the externally observable interface and runtime behavior of the Hall
rotation-speed driver. Signatures below are illustrative of the contract intent;
exact names are finalized during implementation.

## Configuration Contract

- Input: `HallSensorConfig` (pin, active level, pull, debounce, magnets-per-rev,
  stop timeout).
- Defaults: pin = GP15, active-low, internal pull disabled (push-pull output),
  debounce ≈ 1 ms, magnets-per-rev = 1, stop timeout = 1.5 s.
- Behavior:
  - `magnets_per_rev` < 1 is rejected/normalized to 1.
  - Configuration is applied at initialization; the pin is fixed to GP15 by
    default.

## Lifecycle Contract

- `hall_sensor_init(state, config) -> bool`
  - Configures GP15 as input with the requested pull, registers a per-GPIO edge
    interrupt handler, and resets capture state.
  - Returns true on success; false if IRQ registration fails. Must not block.
- `hall_sensor_deinit(state)`
  - Disables the interrupt and releases the handler. Safe to call once.

## Capture Contract

- Each magnet pass that produces a qualifying edge on GP15 is recorded exactly
  once.
- Edges arriving within `debounce_us` of the last accepted edge are ignored.
- The interrupt handler performs only timestamp capture, interval update, and
  count increment; no allocation or heavy computation.

## Measurement Contract

- `hall_sensor_read(state, now_us) -> RotationMeasurement`
  - Non-blocking; safe to call every super-loop iteration.
  - Snapshots interrupt-shared state atomically (critical section) before
    deriving values.
  - `period_us = interval_us * magnets_per_rev`;
    `rpm = 60_000_000 / period_us`; `hz = 1_000_000 / period_us`.
  - `valid` is false until at least two edges have been observed.
  - If `now_us - last_edge_us > stop_timeout_us`, returns `stale = true` with
    `rpm = hz = 0`.
  - Speeds outside the supported range (≈60–6000 RPM) are reported as bounded,
    never as crashes or impossible values.
- Convenience getters MAY expose the latest `rpm`, `hz`, and `period_us`.

## Timing Source Contract

- All derivations use the SDK 64-bit microsecond timebase.
- Only second/minute → microsecond unit conversions are used; no hardcoded
  `clk_sys` frequency literals appear in the timing path.

## Concurrency Contract

- Exactly one edge-producing interrupt source for GP15.
- Shared 64-bit capture state is read under a critical section to prevent torn
  reads; the driver assumes a single reader (the super-loop).
- The driver coexists with the CYW43/RM2 GPIO interrupt usage (per-GPIO raw
  handler, not the shared single-callback API).

## Observability Contract

Required debug events (USB stdio):
- Initialization success/failure and configured pin.
- Periodic speed/period/validity readings while running.
- Stop/stale transitions (rotation stopped) and resumed-rotation transitions.
- Noise/out-of-range rejection events.

## Memory Contract

- Bounded static state only; no heap allocation in init, interrupt, or read
  paths. Static RAM delta stays under the spec ceiling (256 bytes).

## Stability Contract

- Continuous operation for at least 5 minutes of spinning without missed/dup
  counts, hangs, or invalid readings (SC-006).
