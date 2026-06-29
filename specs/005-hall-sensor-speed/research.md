# Research: Hall Sensor Rotation Speed

**Date**: 2026-06-29
**Feature**: 005-hall-sensor-speed

## RES-001: Edge detection mechanism

**Decision**: Detect each magnet pass using a GPIO edge interrupt on GP15 that
captures a timestamp and increments an event count. Register the handler with a
per-GPIO raw IRQ handler (`gpio_add_raw_irq_handler` + `gpio_set_irq_enabled`)
rather than the single shared `gpio_set_irq_enabled_with_callback`.

**Rationale**:
- An interrupt captures edges accurately without polling jitter and without
  blocking the super-loop (FR-008).
- A per-GPIO raw handler coexists with the CYW43/RM2 driver, which also uses the
  GPIO IRQ subsystem; the shared single-callback API would risk clobbering it.
- The interrupt handler stays minimal (timestamp + count + lockout), keeping
  heavy math out of interrupt context (Principle IV).

**Alternatives considered**:
- Polling the pin in the main loop: rejected — can miss short pulses at higher
  speed and adds timing jitter to period measurement.
- PIO-based pulse timing: rejected — overkill for one low-rate digital input, and
  PIO instances are already used by WS2812 and CYW43.

## RES-002: Timebase and speed derivation

**Decision**: Use the SDK 64-bit microsecond timer
(`time_us_64()` / `to_us_since_boot(get_absolute_time())`) for edge timestamps.
Derive values from the interval between consecutive events:
`rev_period_us = interval_us * magnets_per_rev`,
`rpm = 60_000_000 / rev_period_us`, `hz = 1_000_000 / rev_period_us`.

**Rationale**:
- The microsecond timebase gives ~0.01% resolution at 100 Hz (10 000 µs period),
  far exceeding the ±2% accuracy target (SC-001).
- The constants `60_000_000` and `1_000_000` are second/minute → microsecond unit
  conversions, not system-clock frequencies, so no hardcoded `clk_sys` literal is
  introduced (Principle II, FR-009).
- 64-bit timestamps avoid wraparound for the lifetime of the device, satisfying
  FR-011 without special rollover handling.

**Alternatives considered**:
- 32-bit millisecond timestamps: rejected — coarse resolution at high speed and
  wraps in ~49 days; would need explicit rollover handling.
- Deriving from `clock_get_hz(clk_sys)` and a cycle counter: rejected —
  unnecessary complexity; the µs timer is the canonical SDK timebase.

## RES-003: Debounce / noise rejection

**Decision**: Apply a configurable minimum inter-event lockout interval; edges
arriving within the lockout after the last accepted edge are ignored. Default
lockout is derived from the supported maximum speed (well below the 10 ms period
at 6000 RPM), defaulting to ~1 ms.

**Rationale**:
- Guarantees exactly one counted event per magnet pass even if the switch
  chatters near its threshold during a slow approach (FR-002, SC-004).
- A lockout shorter than the minimum legitimate period never rejects a real pass.

**Alternatives considered**:
- No debounce: rejected — threshold dithering on a slow pass could double-count.
- Time-windowed averaging of many edges: rejected — adds latency and complexity
  beyond what a clean Schmitt-trigger Hall switch needs.

## RES-004: Stop / stale detection

**Decision**: In the non-blocking read/update path, if the time since the last
accepted edge exceeds the configurable stop timeout (default 1.5 s), report speed
zero and mark the reading stale.

**Rationale**:
- Matches FR-006 and SC-003/SC-007; avoids holding a stale non-zero value when
  the plate stops.
- A 1.5 s timeout is above the 1 s period of the slowest supported speed (60 RPM)
  so steady low-speed rotation is not falsely declared stopped.

**Alternatives considered**:
- Timer/alarm callback to zero the value: rejected — adds an extra interrupt
  source; the super-loop already polls frequently.

## RES-005: Sensor electrical interface and active level

**Decision**: Treat the HAL250SO as a digital Hall switch with a **push-pull**
output that asserts when a magnet of sufficient field passes. Default
configuration: no internal pull on GP15 (the line is actively driven), treat the
asserted (magnet-present) state as logic low, and count on the falling edge.
Active level, pull setting, and counted edge remain configurable.

**Rationale**:
- The part has a push-pull output, so it drives both logic levels; an internal
  pull resistor is unnecessary and is left disabled by default.
- Keeping level/pull/edge configurable preserves flexibility if the wiring or a
  future part variant differs (e.g., active-high or open-drain).

**Alternatives considered**:
- Enabling an internal pull-up anyway: rejected — unnecessary for a push-pull
  output and could mask wiring faults.

## RES-006: Concurrency between interrupt and reader

**Decision**: Share edge timestamp(s) and the event count between the interrupt
handler and the reader using a brief critical section
(`save_and_disable_interrupts` / `restore_interrupts`, or a `critical_section_t`)
to snapshot the multi-word state atomically before deriving speed.

**Rationale**:
- 64-bit timestamp reads are not atomic on the Cortex-M; a snapshot prevents
  torn reads between the handler and the super-loop.
- The critical section is a few instructions and does not violate the
  non-blocking requirement for normal operation.

**Alternatives considered**:
- `volatile` without a critical section: rejected — torn 64-bit reads can produce
  large transient errors.
- Lock-free sequence-counter pattern: viable but heavier than needed for a single
  reader; kept as a fallback if profiling shows critical-section contention.

## RES-007: Module separation and testability

**Decision**: Split responsibilities: `hall_sensor.cpp` owns GPIO/IRQ capture and
shared state; speed/period derivation is a pure function taking captured state
(timestamps, count, config, "now") and returning the measurement struct.

**Rationale**:
- Satisfies Principle III (hardware abstraction) and makes derivation testable
  with synthetic timestamps, without a spinning plate.
- Keeps consumer logic (logging, future display timing) out of the driver.

**Alternatives considered**:
- One monolithic function doing capture + math in the ISR: rejected — heavier ISR
  and untestable derivation.

## RES-008: Debug observability

**Decision**: Reuse the project's existing USB stdio logging. Emit sensor init
status, periodic speed/period/validity readings, and stop/stale and
out-of-range/noise-rejection transitions.

**Rationale**:
- Required by the spec debug strategy; enables quickstart validation against a
  reference speed without new tooling.

**Alternatives considered**:
- UART-only logging: not selected; current validation flow uses USB stdio.

## RES-009: Pin assignment and conflict check

**Decision**: Use GP15 (GPIO 15) for the Hall input as specified.

**Rationale**:
- GP15 does not conflict with the RM2 wireless pins (36–39, GPIO base 16 PIO) or
  the WS2812 output (GP2), and is a general-purpose input-capable pin on the
  RP2350B.

**Alternatives considered**:
- None — the pin is fixed by the feature request.
