# Feature Specification: WS2812 POV Hello Demo

**Feature Branch**: `004-ws2812-pov-hello`

**Created**: 2026-06-26

**Status**: Draft

**Input**: User description: "Replace blink.pio to a WS2812 pio since we'd like to this project to be a persistence of vision program. There are 57 ws2812 leds at a maximum. When the ws2812 pio driver is ready. Make a demo POV displaying 'Hello' character by character with a duration of one second for each character"

## Clarifications

### Session 2026-06-26

- Q: What should happen after one full Hello sequence completes? -> A: The Hello sequence repeats continuously in a loop.
- Q: When should demo playback start? -> A: Playback starts automatically after WS2812 output-path initialization completes.
- Q: How should demo color and brightness behave across playback? -> A: Character color and brightness remain consistent across the full Hello cycle for repeatable validation.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - WS2812 Output Path Readiness (Priority: P1)

A developer flashes the firmware and confirms the LED output path is ready for addressable LED rendering with a strip size up to 57 pixels.

**Why this priority**: The POV demo cannot run until the LED output path is correct and stable.

**Independent Test**: Connect a WS2812 strip (up to 57 LEDs), boot firmware, and confirm deterministic LED output updates without lockups.

**Acceptance Scenarios**:

1. **Given** firmware is running with a connected WS2812 strip, **When** output updates are generated, **Then** the strip updates reliably without hangs or watchdog resets.
2. **Given** strip length is configured at or below 57 LEDs, **When** output frames are rendered, **Then** all configured LEDs are addressed correctly.

---

### User Story 2 - POV Hello Playback (Priority: P1)

A user starts the built-in demo and sees the word Hello displayed one character at a time.

**Why this priority**: This is the primary user-visible value requested for the feature.

**Independent Test**: Run demo playback and observe H, e, l, l, o shown sequentially with one second display duration per character.

**Acceptance Scenarios**:

1. **Given** the demo is started, **When** playback begins, **Then** the first displayed character is H.
2. **Given** demo playback is active, **When** each character period elapses, **Then** the next character in the sequence H, e, l, l, o is shown.
3. **Given** a character is displayed, **When** timing is measured, **Then** each character remains visible for 1.0 seconds plus or minus 0.1 seconds.

---

### User Story 3 - Safe Bounds and Fallback Behavior (Priority: P2)

A developer configures LED count or demo state and expects safe behavior at limits and invalid conditions.

**Why this priority**: Preventing out-of-range addressing is required for stable operation on hardware.

**Independent Test**: Exercise boundary values (0, 1, 57, and >57) and verify bounded behavior.

**Acceptance Scenarios**:

1. **Given** requested LED count exceeds 57, **When** runtime initializes output, **Then** the system clamps the value to 57 and continues operating safely.
2. **Given** output is temporarily unavailable, **When** demo scheduling runs, **Then** the system logs a recoverable error state and remains responsive.

---

### Edge Cases

- LED count configured as zero or negative-equivalent input.
- LED count configured above 57.
- Character playback timing drift under sustained runtime.
- Output signal initialization fails at startup.
- Demo restarts while playback is in progress.
- Sequence reaches the final character o and must transition back to H cleanly.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST support addressable LED output suitable for persistence-of-vision rendering.
- **FR-002**: The system MUST support a maximum configured strip length of 57 WS2812 LEDs.
- **FR-003**: The system MUST prevent out-of-range LED addressing when a requested length exceeds 57.
- **FR-004**: The system MUST provide a demo sequence that displays the text Hello character by character in the exact order H, e, l, l, o.
- **FR-005**: The demo MUST display each character for 1.0 seconds plus or minus 0.1 seconds.
- **FR-006**: The system MUST keep timing-stable playback during continuous demo operation.
- **FR-007**: The system MUST expose observable runtime status for demo start, character transition, and bounded-error conditions.
- **FR-008**: The feature MUST preserve existing build and flash workflow with single-command build behavior.
- **FR-009**: The Hello demo MUST start automatically once the WS2812 output path is ready after boot.
- **FR-010**: After displaying o, the demo MUST continue by displaying H again in a continuous loop until stopped or reset.
- **FR-011**: Character color and brightness MUST remain consistent across each full Hello playback cycle.
- **FR-012**: The LED transfer path MUST use a DMA -> TX FIFO -> PIO pipeline for frame emission without CPU bit-banging.
- **FR-013**: Output timing parameters MUST be derived at runtime from `clock_get_hz(clk_sys)` and MUST NOT use hardcoded system-clock constants.
- **FR-014**: Development validation builds for this feature MUST use USB stdio for runtime logs; release builds MUST keep stdio disabled unless explicitly justified.

### Key Entities *(include if feature involves data)*

- **LedStripConfig**: Runtime strip configuration including active LED count and bounds status.
- **PovDemoSequence**: Ordered character sequence and per-character duration metadata for Hello playback.
- **PovPlaybackState**: Current character index, transition timestamp, and running/stopped state.
- **OutputHealthState**: Output-path readiness and recoverable error indicators.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Demo playback shows H, e, l, l, o in the correct order for 100 percent of measured runs.
- **SC-002**: Each character display duration is 1.0 seconds plus or minus 0.1 seconds across a full Hello cycle.
- **SC-003**: With LED count set to 57, the system remains stable for at least 5 minutes of continuous demo playback.
- **SC-004**: Invalid LED count input above 57 results in bounded safe behavior with no crashes or hangs.
- **SC-005**: After output-path readiness, the first character H appears within 2 seconds on boot.
- **SC-006**: At least 3 consecutive Hello cycles complete with correct ordering and no missing transitions.
- **SC-007**: Validation evidence confirms frame transmission over DMA-backed PIO path during Hello playback with no CPU bit-banging fallback.
- **SC-008**: Timing-configuration evidence confirms runtime derivation via `clock_get_hz(clk_sys)` and zero hardcoded system-clock literals in WS2812 timing code.

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|---------------|---------------|
| I. PIO-First LED Drive | Applies | Timing-critical LED output uses a DMA -> TX FIFO -> PIO transfer path, not CPU bit-banging. |
| II. Timing Precision | Applies | Timing configuration is derived from `clock_get_hz(clk_sys)` and validated against the required cadence/tolerance. |
| III. Hardware Abstraction | Applies | Output-path logic and demo-sequence logic are kept as separate responsibilities. |
| IV. Minimal and Deterministic Memory Use | Applies | Static memory usage for playback and frame data is bounded and tracked. |
| V. Single-Command Build and Flash | Applies | Feature keeps the existing single-command build and current flashing workflow. |

## Debug Output Strategy

- Development validation for this feature uses USB stdio so demo transitions and error states are observable.
- Release builds keep USB/UART stdio disabled unless a release note explicitly justifies enabling them.
- Required logs include demo start, each character transition, active LED count, and bounded-error events.

## Static RAM Budget

Measured with `arm-none-eabi-gcc` (Cortex-M0+, `-fshort-enums` per Arm EABI default), matching the production build toolchain.

| Component | Count | Bytes each | Total bytes |
|-----------|-------|------------|-------------|
| WS2812 frame buffer (`static uint32_t frame_words[57]`) | 57 | 4 | 228 |
| `ws2812_driver_t` instance (incl. `led_strip_config_t` 4 B + `output_health_state_t` 12 B) | 1 | 52 | 52 |
| `pov_demo_t` instance (incl. `pov_demo_sequence_t` 10 B + `pov_playback_state_t` 12 B) | 1 | 24 | 24 |
| Main-loop scratch state (timestamps + status flags) | 1 | 16 | 16 |
| **Feature static RAM total (measured)** |  |  | **320 bytes** |

- The feature uses a bounded RAM total of 320 measured bytes.
- The frame buffer (228 B) is the only `static`-storage allocation; the `ws2812_driver_t`, `pov_demo_t`, and scratch state are `main()`-stack resident for the program lifetime.
- Per-struct measured sizes: `led_strip_config_t` = 4 B, `output_health_state_t` = 12 B, `ws2812_driver_t` = 52 B, `pov_demo_sequence_t` = 10 B, `pov_playback_state_t` = 12 B, `pov_demo_t` = 24 B.
- The total must remain under 512 bytes without explicit spec revision.
- Implementation evidence and any future variance are recorded in `validation.md`.

## Assumptions

- The target hardware remains Raspberry Pi Pico W with a connected WS2812 strip.
- Demo scope is limited to the single text sequence Hello for this feature.
- Demo starts automatically after output-path initialization and loops continuously.
- Multi-word text rendering, custom fonts, and user-configurable message editing are out of scope.
- Existing project networking features are unchanged by this feature unless explicitly required in later phases.
