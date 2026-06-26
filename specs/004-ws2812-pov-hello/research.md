# Research: WS2812 POV Hello Demo

**Date**: 2026-06-26
**Feature**: 004-ws2812-pov-hello

## RES-001: Replace blink PIO with WS2812 PIO driver

**Decision**: Replace the current `blink.pio` program with a WS2812-oriented PIO program and generated header that supports streaming 24-bit GRB pixel data for up to 57 LEDs.

**Rationale**:
- The existing blink PIO only toggles one GPIO and cannot represent multi-pixel color frames.
- POV rendering needs deterministic serial timing to each LED; PIO is the correct timing source per constitution.
- Keeping the PIO source in `blink.pio` preserves current build wiring (`pico_generate_pio_header`) and minimizes structural churn.

**Alternatives considered**:
- Keep blink PIO and bit-bang WS2812 from CPU: rejected due to timing instability and constitution conflict.
- Introduce a second `.pio` file while retaining blink: rejected for this feature scope; replacement is explicitly requested.

## RES-002: Frame update strategy for POV Hello

**Decision**: Use a fixed-size, statically allocated LED frame buffer sized for the configured LED count (capped at 57), and update frame contents for each character transition.

**Rationale**:
- Statically bounded memory satisfies deterministic memory constraints.
- Character-by-character transitions at 1-second intervals are low-rate and suitable for simple frame swapping.
- A fixed buffer avoids allocation and fragmentation risks on RP2040.

**Alternatives considered**:
- Dynamic frame objects per character: rejected due to heap usage in the display path.
- Precompute all character frames as large immutable tables: rejected to reduce static flash/RAM pressure.

## RES-003: Playback timing and sequence control

**Decision**: Drive character transitions using monotonic runtime timestamps and a 1-second cadence target with a tolerance envelope of +/-100 ms.

**Rationale**:
- Matches clarified requirement and success criteria.
- Timestamp-based scheduling is robust to loop jitter and easy to instrument in logs.
- Integrates with existing superloop model without blocking delays.

**Alternatives considered**:
- `sleep_ms(1000)` between characters: rejected because blocking delays can interfere with other runtime services.
- Hardware timer interrupt-driven state machine: deferred; not required for this demo scope.

## RES-004: Startup and looping behavior

**Decision**: Auto-start demo once WS2812 output initialization succeeds, then loop continuously through H, e, l, l, o.

**Rationale**:
- Aligns with clarified behavior and avoids ambiguous idle state.
- Continuous loop provides repeatable validation windows for stability and timing checks.

**Alternatives considered**:
- One-shot playback: rejected by clarified preference.
- Manual trigger only: rejected as out of scope for current feature request.

## RES-005: Debug observability

**Decision**: Keep USB stdio enabled for development validation and log initialization, active LED count, character transitions, and bounded-error conditions.

**Rationale**:
- Required by spec debug strategy.
- Allows measuring startup latency and transition timing during quickstart validation.
- No new output transport is needed.

**Alternatives considered**:
- UART-only logs: not selected for this feature because current project validation flow already uses USB.
