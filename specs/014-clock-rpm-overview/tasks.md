# Tasks: Clock and RPM Overview

**Input**: Design documents from `specs/014-clock-rpm-overview/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md,
contracts/overview-runtime-status.md, quickstart.md

## Phase 1: Setup

- [X] T001 Create build, page-render, memory, and hardware evidence sections in `specs/014-clock-rpm-overview/validation.md`

---

## Phase 2: Foundational

- [X] T002 Add bounded clock-status publication contract in `wifi_config/wifi_config.h`
- [X] T003 [P] Extend STA runtime-status contract with clock availability/text in `wifi_config/wifi_sta_http.h`
- [X] T004 [P] Extend Overview builder contract with clock availability/text in `wifi_config/wifi_sta_web.h`

**Checkpoint**: All module boundaries represent the same bounded clock snapshot.

---

## Phase 3: User Story 1 - Check Clock and Speed Remotely (Priority: P1) MVP

**Goal**: A refreshed Overview independently displays current CST and Hall RPM.

**Independent Test**: Render Overview with calibrated/unavailable clock crossed
with available/unavailable RPM and verify exact values/placeholders while all
existing metrics remain present.

- [X] T005 [US1] Store and forward bounded clock status alongside RPM in `wifi_config/wifi_config.c`
- [X] T006 [US1] Retain HTTP-facing clock status and pass it into Overview rendering in `wifi_config/wifi_sta_http.c`
- [X] T007 [US1] Render Current Clock with calibrated and placeholder states while preserving Rotation Speed in `wifi_config/wifi_sta_web.c`
- [X] T008 [US1] Publish calibrated `HH:MM:SS` status from the existing clock state each loop in `pov_leds.cpp`

**Checkpoint**: The P1 refresh workflow works without USB and without browser polling.

---

## Phase 4: Polish & Cross-Cutting Concerns

- [X] T009 Run host page-builder checks for clock/RPM state combinations and record evidence in `specs/014-clock-rpm-overview/validation.md`
- [X] T010 Run `ninja -C build`, record fixed static-RAM impact and pending hardware checks in `specs/014-clock-rpm-overview/validation.md`, and mark completed tasks in `specs/014-clock-rpm-overview/tasks.md`

---

## Dependencies & Execution Order

- T001 starts independently.
- T002 establishes naming; T003 and T004 can then proceed in parallel.
- T005 -> T006 -> T007 forms the runtime-to-presentation path.
- T008 depends on T002 and joins the completed status path for integration.
- T009 and T010 follow the full P1 implementation.

## Parallel Example: User Story 1

```text
After the contracts are stable:
- Implement bounded runtime/HTTP copies in wifi_config/*.c.
- Implement Current Clock rendering in wifi_config/wifi_sta_web.c.
- Implement clock publication in pov_leds.cpp.
```

## Implementation Strategy

1. Establish the bounded clock status contract and evidence log.
2. Extend the existing RPM status pipeline end to end.
3. Verify independent unavailable/available combinations, build health, and memory.

## Format Validation

All tasks use checkboxes, sequential IDs, appropriate phase/story labels, and
exact file paths.

