# Feature Specification: Color HHMMSS Clock

**Feature Branch**: `010-color-hhmmss-clock`

**Created**: 2026-07-10

**Status**: Draft

**Input**: User description: "No need to display \"CST\", only HH:MM:SS should be enough. I'd like to make HH in red color, MM in gree and SS in blue. As always, you can do all remaining speckit procedures autonomously except git commit. Finally, generate a JPEG preview as well"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read a Simpler Clock Format (Priority: P1)

As a viewer, I want the POV clock to show only `HH:MM:SS`, so that the time is larger and easier to understand while the PCB is spinning.

**Why this priority**: The previous `HH:MM:SS CST` layout is visually dense. Removing the timezone label directly improves readability without changing time calibration or rotation behavior.

**Independent Test**: With calibrated time and suitable rotation, observe the display and confirm that the visible text contains exactly hours, minutes, and seconds separated by colons, with no timezone label.

**Acceptance Scenarios**:

1. **Given** the clock is in normal display mode, **When** the time is rendered, **Then** the visible text is formatted as `HH:MM:SS`.
2. **Given** the display is running, **When** the seconds value changes, **Then** the rendered text updates once per wall-clock second.
3. **Given** the display is previewed as a JPEG, **When** the preview is inspected, **Then** no `CST` letters appear in the clock text.

---

### User Story 2 - Distinguish Time Components by Color (Priority: P1)

As a viewer, I want hours, minutes, and seconds to use different colors, so that I can quickly separate the three time components at a glance.

**Why this priority**: Color separation is the requested visual improvement and makes the simplified display easier to parse.

**Independent Test**: Render a preview or run the device with a known time and confirm that the hours digits are red, minutes digits are green, and seconds digits are blue.

**Acceptance Scenarios**:

1. **Given** the display shows `HH:MM:SS`, **When** the hour digits are visible, **Then** both hour digits use red.
2. **Given** the display shows `HH:MM:SS`, **When** the minute digits are visible, **Then** both minute digits use green.
3. **Given** the display shows `HH:MM:SS`, **When** the second digits are visible, **Then** both second digits use blue.
4. **Given** the display contains colons, **When** colons are rendered, **Then** they use a neutral separator color that does not obscure the red, green, or blue groups.

---

### User Story 3 - Review Before Hardware Run (Priority: P2)

As the builder, I want a generated JPEG preview of the simplified colored layout, so that I can inspect the intended appearance before running the device on hardware.

**Why this priority**: A preview reduces hardware iteration time and makes layout decisions visible before flashing.

**Independent Test**: Open the generated JPEG preview and confirm it shows both the unwrapped frame buffer and circular POV projection for a sample `HH:MM:SS` value.

**Acceptance Scenarios**:

1. **Given** the implementation is complete, **When** the preview is generated, **Then** the JPEG exists in the feature directory.
2. **Given** the JPEG is opened, **When** the preview is inspected, **Then** the unwrapped preview and circular projection both show red hours, green minutes, and blue seconds.

### Edge Cases

- The time rolls from `09:59:59` to `10:00:00`; both hour digits remain red and both minute/second groups retain their colors.
- The time rolls from `23:59:59` to `00:00:00`; the display remains `HH:MM:SS` and colors do not shift.
- A colon column appears between colored groups; it must remain readable without being confused as part of a digit group.
- The display enters a non-clock status state; status colors remain separate from the normal time-component colors.
- Preview generation runs without hardware attached.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The normal clock display MUST show time as `HH:MM:SS` only.
- **FR-002**: The normal clock display MUST NOT include the `CST` timezone label.
- **FR-003**: The hour digits MUST render in red.
- **FR-004**: The minute digits MUST render in green.
- **FR-005**: The second digits MUST render in blue.
- **FR-006**: The colon separators MUST render in a neutral separator color.
- **FR-007**: The clock MUST continue updating exactly once per wall-clock second while time and rotation are valid.
- **FR-008**: The change MUST preserve the existing UTC+8 time calibration behavior from the previous clock feature.
- **FR-009**: The change MUST preserve existing rotation suitability and invalid-state behavior.
- **FR-010**: The implementation MUST keep display memory bounded and avoid dynamic allocation in the render path.
- **FR-011**: A JPEG preview MUST be generated showing the simplified colored layout before completion.
- **FR-012**: The existing single-command build workflow MUST remain valid.

### Key Entities *(include if feature involves data)*

- **Colored Clock Value**: The visible `HH:MM:SS` text and the color assignment for each time component.
- **Time Component Color Map**: The mapping from hours to red, minutes to green, seconds to blue, and separators to neutral.
- **Preview Artifact**: A JPEG image showing the unwrapped frame buffer and approximate circular projection for a sample colored time.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The visible normal clock text contains exactly 8 characters in `HH:MM:SS` format for 100% of normal display updates.
- **SC-002**: No `CST` letters appear in firmware-rendered clock text or the generated preview.
- **SC-003**: Hours, minutes, and seconds use red, green, and blue respectively in 100% of rendered digit columns.
- **SC-004**: The generated JPEG preview is created and opens successfully.
- **SC-005**: The firmware builds successfully with the existing `ninja -C build` command.
- **SC-006**: The display still changes seconds exactly once per wall-clock second when time and rotation are valid.

## Constitution Compliance

| Principle | Applicability | How Satisfied |
|-----------|---------------|---------------|
| I. PIO-First LED Drive | Applies | The feature changes only prepared pixel content; LED transmission remains DMA -> TX FIFO -> PIO. |
| II. Timing Precision | Applies | The feature preserves measured-rotation column timing and once-per-second time updates. |
| III. Hardware Abstraction | Applies | Color/text layout remains in display-rendering logic, separate from Wi-Fi time and hardware drivers. |
| IV. Minimal and Deterministic Memory Use | Applies | The feature only changes bounded renderer state and preview artifacts; no heap allocation is introduced in the display path. |
| V. Single-Command Build and Flash | Applies | The build continues to use the existing `ninja -C build` workflow. |

## Assumptions

- The previous feature's UTC+8 calibration remains correct; removing the label does not change the time zone used internally.
- The neutral separator color for colons defaults to white so the separators stay visible between colored groups.
- The sample preview time can be `12:34:56`, matching the previous preview convention.
- Hardware validation remains manual; this feature must at least provide build validation and a generated preview.
