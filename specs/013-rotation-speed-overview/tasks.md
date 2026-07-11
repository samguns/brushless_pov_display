# Tasks: Rotation Speed Overview

**Input**: Design documents from `specs/013-rotation-speed-overview/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/rotation-speed-status.md, quickstart.md

## Phase 1: Setup

**Purpose**: Prepare traceable validation for the focused runtime/UI change.

- [X] T001 Create the build and scenario evidence scaffold in `specs/013-rotation-speed-overview/validation.md`

---

## Phase 2: Foundational

**Purpose**: Define the fixed-size status contract that all story work uses.

- [X] T002 Add rotation-speed publication declarations and documentation in `wifi_config/wifi_config.h`
- [X] T003 [P] Extend the STA runtime-status declaration with availability and whole RPM in `wifi_config/wifi_sta_http.h`
- [X] T004 [P] Extend the Overview page-builder declaration with availability and whole RPM in `wifi_config/wifi_sta_web.h`

**Checkpoint**: Rotation status can be represented consistently at every module boundary.

---

## Phase 3: User Story 1 - View Current Rotation Speed (Priority: P1) MVP

**Goal**: Show request-time whole-number RPM on Overview with distinct unavailable
and stopped behavior.

**Independent Test**: Exercise the builder/runtime path with unavailable, valid
rotating, and stopped status; confirm Overview renders `-- RPM`, rounded
`<n> RPM`, and `0 RPM` respectively while retaining existing metrics.

- [X] T005 [US1] Store and forward the fixed-size rotation status through `wifi_config/wifi_config.c`
- [X] T006 [US1] Store HTTP-facing rotation status and pass it into Overview rendering in `wifi_config/wifi_sta_http.c`
- [X] T007 [US1] Render the `Rotation Speed` metric and its unavailable/value states in `wifi_config/wifi_sta_web.c`
- [X] T008 [US1] Publish nearest-whole RPM and preserve first-valid versus stopped state in `pov_leds.cpp`

**Checkpoint**: The complete user story is independently usable from the STA Overview page.

---

## Phase 4: Polish & Cross-Cutting Concerns

**Purpose**: Verify requirements, memory constraints, and end-to-end build health.

- [X] T009 Validate unavailable, rotating, stopped, and responsive-layout scenarios and record available evidence in `specs/013-rotation-speed-overview/validation.md`
- [X] T010 Run `ninja -C build`, record results and the fixed static-RAM delta in `specs/013-rotation-speed-overview/validation.md`, and update completed items in `specs/013-rotation-speed-overview/tasks.md`

---

## Dependencies & Execution Order

- Phase 1 has no dependencies.
- Phase 2 depends on T001; T003 and T004 can proceed in parallel after T002 establishes naming.
- User Story 1 depends on Phase 2. T005 -> T006 -> T007 is the transport/render path;
  T008 can proceed once T002 is complete and joins the path for integration validation.
- Polish depends on User Story 1 completion.

## Parallel Example: User Story 1

```text
After declarations are stable:
- Implement the wifi_config -> HTTP propagation in wifi_config/*.c
- Implement Overview metric formatting in wifi_config/wifi_sta_web.c
- Implement Hall measurement publication in pov_leds.cpp
```

## Implementation Strategy

1. Establish the two-field status contract and validation log.
2. Complete the single P1 story end to end as the MVP.
3. Build, inspect generated page behavior where host-verifiable, and leave the
   hardware-only scenarios explicitly recorded for device validation.

## Format Validation

All implementation tasks use a checkbox, sequential task ID, appropriate phase
label, and exact target file path.

