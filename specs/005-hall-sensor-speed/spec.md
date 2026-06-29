# Feature Specification: Hall Sensor Rotation Speed

**Feature Branch**: `005-hall-sensor-speed`

**Created**: 2026-06-29

**Status**: Draft

**Input**: User description: "Write a hall sensor (refer to https://item.szlcsc.com/datasheet/HAL250SO/8400241.html ) connected to GP15. Write a driver that I can get the spinning speed when the board is placed on a spinning plate"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read Spinning Speed (Priority: P1)

As a builder running the board on a spinning plate, I want to read how fast the
plate is rotating so that I know the current spin rate at any moment.

A single reference magnet is mounted so that it passes the Hall sensor once per
revolution. As the plate spins, the sensor produces one detectable event per
turn. The driver measures the time between events and reports the spin rate.

**Why this priority**: Reading the spin rate is the entire purpose of the
feature and the minimum deliverable that provides value. Every other capability
builds on having a usable speed measurement.

**Independent Test**: Mount the board on a plate spinning at a known steady rate,
read the reported speed, and confirm it matches the reference rate within
tolerance. Delivers a usable rotation-speed reading on its own.

**Acceptance Scenarios**:

1. **Given** the plate spins at a steady known rate within the supported range,
   **When** the speed is read, **Then** the reported value matches the actual
   rate within the accuracy tolerance.
2. **Given** the plate is spinning, **When** the spin rate increases or
   decreases, **Then** the reported speed follows the change within a few
   revolutions.
3. **Given** the board is powered but the plate is not spinning, **When** the
   speed is read, **Then** the reported speed is zero.

---

### User Story 2 - Continuous Non-Blocking Speed Access (Priority: P2)

As a developer integrating other features (e.g., timing the persistence-of-vision
display), I want to query the latest spin rate and rotation period at any time
without stalling the main loop, so that rendering and logging can stay in sync
with rotation.

**Why this priority**: The project's display timing depends on the measured
rotation period. Exposing a continuously updated, non-blocking reading is what
makes the measurement useful to the rest of the firmware, but it depends on US1
existing first.

**Independent Test**: While the plate spins, repeatedly query the latest speed
and rotation period from the main loop and confirm the values update over time
and that querying never blocks or pauses other work.

**Acceptance Scenarios**:

1. **Given** the plate is spinning steadily, **When** the latest reading is
   queried repeatedly, **Then** each query returns promptly with the most recent
   speed and rotation period.
2. **Given** other periodic work is running in the main loop, **When** rotation
   measurement is active, **Then** the measurement does not introduce blocking
   delays that disrupt that work.
3. **Given** a fresh measurement has been taken, **When** the reading is queried,
   **Then** the reading indicates whether it is currently valid/fresh or stale.

---

### User Story 3 - Robust Behavior at Boundaries (Priority: P3)

As an operator, I want the driver to behave safely when rotation stops, when the
signal is noisy, or when the speed is outside the expected range, so that the
system never hangs or reports nonsense.

**Why this priority**: Correctness at the edges protects every consumer of the
measurement, but it is only meaningful once the core measurement (US1) and the
access path (US2) exist.

**Independent Test**: Exercise stopped rotation, a single slow pass, rapid
passes near the upper limit, and contact bounce/noise near the threshold, and
confirm the reported speed stays bounded, returns to zero on stop, and counts
exactly one event per magnet pass.

**Acceptance Scenarios**:

1. **Given** the plate was spinning and then stops, **When** no magnet pass
   occurs for longer than the stop-detection timeout, **Then** the reported speed
   becomes zero and is marked stale.
2. **Given** electrical noise or contact bounce around the switching threshold,
   **When** the magnet passes once, **Then** exactly one rotation event is
   counted.
3. **Given** the plate spins faster or slower than the supported range, **When**
   the speed is read, **Then** the driver reports a bounded result without
   crashing, hanging, or producing wildly incorrect values.

### Edge Cases

- **Stationary board**: No events arrive; speed must read zero after the
  stop-detection timeout rather than holding the last value forever.
- **First revolution after start**: Speed is unknown until at least two events
  (or one full period) are observed; the reading must be marked invalid until
  then.
- **Timestamp wraparound**: The running time reference rolls over during long
  operation; period calculation must remain correct across the rollover.
- **Contact bounce / threshold noise**: A single magnet pass must not be counted
  as multiple events.
- **Multiple magnets**: If more than one magnet is installed per revolution, the
  conversion from events to revolutions must account for the configured count.
- **Sensor signal stuck**: A line that is permanently asserted or never asserts
  must result in a zero/invalid reading, not a hang.
- **Very high speed**: Events arriving faster than the supported maximum must be
  clamped/ignored rather than producing impossible readings.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST sense the digital output of the Hall sensor
  connected to GP15.
- **FR-002**: The system MUST detect each magnet pass as a single discrete
  rotation event, rejecting spurious extra transitions caused by bounce or noise.
- **FR-003**: The system MUST measure the elapsed time between consecutive
  rotation events to determine the rotation period.
- **FR-004**: The system MUST compute the spinning speed and expose it in both
  revolutions per minute (RPM) and revolutions per second (Hz).
- **FR-005**: The system MUST allow other firmware modules to read the latest
  spinning speed and rotation period through a driver interface.
- **FR-006**: The system MUST report a spinning speed of zero when no rotation
  event has occurred within the stop-detection timeout.
- **FR-007**: The system MUST support a configurable number of magnet passes per
  revolution, defaulting to one.
- **FR-008**: The measurement MUST operate without blocking the main loop or
  introducing busy-wait delays in the rotation path.
- **FR-009**: All timing calculations MUST be derived from the runtime system
  clock source rather than hardcoded clock constants.
- **FR-010**: The system MUST indicate whether the current reading is valid/fresh
  or stale.
- **FR-011**: The system MUST correctly handle wraparound of the time reference
  used for period measurement.
- **FR-012**: The system MUST use bounded, statically allocated state with no
  dynamic memory allocation in the measurement path.
- **FR-013**: The system MUST keep rotation-event detection and speed-derivation
  logic separated from higher-level display/consumer logic.

### Key Entities *(include if feature involves data)*

- **Rotation Sample**: A detected magnet-pass event with its capture timestamp;
  the basis for period and speed calculation.
- **Rotation Measurement State**: The current derived values — latest rotation
  period, speed in RPM and Hz, validity/freshness flag, and last-event timestamp.
- **Hall Sensor Configuration**: The sensing parameters — input pin (GP15),
  active signal level, debounce interval, magnets-per-revolution, and
  stop-detection timeout.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: When the plate spins at a steady known rate within the supported
  range, the reported speed is within ±2% of the actual rate.
- **SC-002**: After a sustained change in spin rate, the reported speed settles
  to the new value within 3 revolutions.
- **SC-003**: When rotation stops, the reported speed returns to zero within the
  stop-detection timeout of 1.5 seconds.
- **SC-004**: Across a continuous test of at least 500 magnet passes, exactly one
  rotation event is counted per pass (no missed or duplicated counts).
- **SC-005**: The latest reading can be queried from the main loop with no
  `sleep` or busy-wait in the read path.
- **SC-006**: The driver runs continuously for at least 5 minutes of spinning
  without drift in counting, hangs, or invalid readings.
- **SC-007**: When the plate is stationary, the reported speed reads zero and the
  reading is marked stale 100% of the time.

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|---------------|---------------|
| I. PIO-First LED Drive | Indirect | This feature provides the measured rotation period that the PIO-driven LED output uses for column timing; it does not change the LED output path. |
| II. Timing Precision | Applies | Rotation period and speed are derived from `clock_get_hz(clk_sys)` at runtime with no hardcoded clock literals, providing the timing basis the display depends on. |
| III. Hardware Abstraction | Applies | Hall-sensor input handling and event capture are kept separate from speed-derivation and consumer logic in dedicated module(s). |
| IV. Minimal and Deterministic Memory Use | Applies | Measurement uses bounded static state only; no heap allocation in the measurement path. RAM impact is tracked below. |
| V. Single-Command Build and Flash | Applies | The feature builds with the existing `ninja -C build` flow and current flashing workflow with no out-of-band steps. |

## Debug Output Strategy

- Development validation uses the project's existing USB stdio logging so the
  measured speed, rotation period, and validity can be observed while spinning.
- Required logs include rotation-measurement start, periodic speed/period
  readings, stop/stale transitions, and out-of-range or noise-rejection events.
- Release builds keep stdio disabled unless a release note explicitly justifies
  enabling it.

## Static RAM Budget

Measured with `arm-none-eabi-gcc` (Cortex-M33, RP2350), matching the build toolchain.

| Component | Bytes |
|-----------|-------|
| `hall_sensor_t` instance (incl. `hall_sensor_config_t` 16 B + `hall_capture_t` 32 B) | 56 |
| Main-loop scratch (log timestamp + status flags) | ~8 |
| **Feature RAM total (measured)** | **~64** |

- Per-struct measured sizes: `hall_sensor_config_t` = 16 B, `hall_capture_t` = 32 B,
  `hall_rotation_measurement_t` = 24 B (transient per-read result), `hall_sensor_t` = 56 B.
- The `hall_sensor_t` instance is the only persistent allocation; the measurement
  struct is a transient stack value returned by each read.
- The static RAM delta stays well under the 256-byte ceiling without spec revision.

## Assumptions

- The HAL250SO behaves as a digital Hall-effect switch with a **push-pull**
  output that actively drives its pin to a defined logic level when a magnet of
  sufficient field strength passes, suitable for one-event-per-pass rotation
  sensing.
- A single reference magnet is fixed relative to the spinning plate so the sensor
  passes it once per revolution; the default magnets-per-revolution is 1.
- The supported measurement range is approximately 60 RPM to 6000 RPM (1 Hz to
  100 Hz); rates outside this range are reported as bounded/zero rather than
  precise values.
- GP15 (GPIO 15) is available on the target board and does not conflict with the
  wireless module pins or the WS2812 output pin already in use.
- Because the output is push-pull, the line is actively driven in both states, so
  no internal or external pull resistor is required on GP15.
- The target board remains the project's RP2350-based board with the existing
  firmware loop into which this driver is integrated.
- This feature measures rotation speed only; using the measurement to drive
  persistence-of-vision column timing is a separate feature.
