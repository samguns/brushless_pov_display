# Tasks: WiFi Firmware Update

**Input**: Design documents from `/specs/011-wifi-firmware-update/`

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), and [firmware-update HTTP contract](contracts/firmware-update-http.md)

**Tests**: Hardware validation is required by the specification. Add host tests for the
package envelope/parser where feasible; the firmware target has no existing host unit-test
framework.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Pin the bootloader source and introduce repeatable package production.

- [X] T001 Vendor a pinned MIT-licensed `pico_fota_bootloader` revision and its LICENSE in deps/pico_fota_bootloader/
- [X] T002 Add the FOTA dependency, 8 KB aligned reserved tail (final 4 KB settings), bootloader build, and FOTA artifact targets in CMakeLists.txt
- [X] T003 [P] Create the deterministic `.povota` envelope generator in tools/package_firmware.py
- [X] T004 [P] Add host coverage for valid, corrupt, truncated, and wrong-board package envelopes in tools/test_package_firmware.py

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Provide a bounded, hardware-independent update state machine and preserve
existing settings before portal behavior is changed.

- [X] T005 Define package, session, result, and status interfaces in wifi_config/wifi_firmware_update.h
- [X] T006 Implement bounded envelope parsing, 256-byte alignment, FOTA slot initialization, write, SHA validation, invalidation, and restart APIs in wifi_config/wifi_firmware_update.c
- [X] T007 Preserve the final 4 KB credential/token/brightness sector within the FOTA 8 KB aligned tail in wifi_config/wifi_flash.c
- [X] T008 [P] Add compile-time firmware build identity and board ID configuration in CMakeLists.txt
- [X] T009 Build the bootloader/application/package outputs and inspect the slot/settings boundary with CMakeLists.txt

**Checkpoint**: FOTA primitives and persistent settings layout are available; user stories
can safely use them.

---

## Phase 3: User Story 1 - Upload Firmware from the Web UI (Priority: P1) 🎯 MVP

**Goal**: An owner can submit a matching package from the normal STA portal, see progress,
and restart into a successfully committed application without USB.

**Independent Test**: Build matching `.povota`, upload it from `/update`, verify advancing
status and automatic reconnect with retained WiFi credentials/settings.

- [ ] T010 [US1] Replace the legacy USB-only update page with authenticated upload, warning, progress, and restart markup in wifi_config/wifi_sta_web.c
- [ ] T011 [US1] Implement `GET /update` and pollable `GET /update/status` responses in wifi_config/wifi_sta_http.c
- [ ] T012 [US1] Add bounded streaming multipart request handling, token enforcement, and single-session conflict handling for `POST /update` in wifi_config/wifi_sta_http.c
- [ ] T013 [US1] Automatically mark a fully validated slot valid, flush its final response, and defer FOTA restart in wifi_config/wifi_sta_http.c
- [ ] T014 [US1] Commit a healthy first boot and expose FOTA rollback/update result during startup in pov_leds.cpp
- [ ] T015 [US1] Display the generated build identity after restart in wifi_config/wifi_sta_web.c
- [ ] T016 [US1] Preserve the authorized USB BOOTSEL fallback action at `POST /update/usb-recovery` in wifi_config/wifi_sta_http.c
- [ ] T017 [US1] Validate the successful WiFi upload/reconnect scenario on hardware using specs/011-wifi-firmware-update/quickstart.md

**Checkpoint**: A valid board-matched package updates over WiFi and returns to normal
operation, with no USB interaction after the one-time migration.

---

## Phase 4: User Story 2 - Reject Invalid or Incompatible Firmware (Priority: P1)

**Goal**: Invalid, corrupt, truncated, oversize, and wrong-board files are rejected before
they can change the running image.

**Independent Test**: Submit each invalid class and confirm a clear error, unchanged current
build, and retained portal availability.

- [ ] T018 [US2] Map malformed, empty, oversize, envelope-CRC, board-ID, and FOTA-SHA errors to explicit session results in wifi_config/wifi_firmware_update.c
- [ ] T019 [US2] Return contract-compliant `400`, `401`, `409`, `422`, and throttled authorization responses from wifi_config/wifi_sta_http.c
- [ ] T020 [US2] Render plain-language retry/recovery errors without secret data in wifi_config/wifi_sta_web.c
- [ ] T021 [US2] Run invalid-file, corrupted, truncated, oversize, wrong-board, unauthorized, and duplicate-submission validation in specs/011-wifi-firmware-update/quickstart.md

**Checkpoint**: Every specified invalid package leaves the prior firmware running and reports
why it was rejected.

---

## Phase 5: User Story 3 - Recover Safely from Interrupted Updates (Priority: P2)

**Goal**: Interrupted uploads are discarded and uncommitted candidate boots roll back;
owners retain a clear USB recovery path.

**Independent Test**: Disconnect during upload and reset during candidate boot; verify the
previous image remains/reruns and recovery instructions are available.

- [ ] T022 [US3] Abort and invalidate partial downloads on socket close, stalled upload, WiFi loss, and parser failure in wifi_config/wifi_sta_http.c
- [ ] T023 [US3] Surface rollback state and USB BOOTSEL recovery instructions in wifi_config/wifi_sta_web.c
- [ ] T024 [US3] Verify settings survive download failure, successful update, rollback, and USB recovery in wifi_config/wifi_flash.c
- [ ] T025 [US3] Perform interrupted-upload, power/reset-before-commit, rollback, and USB-recovery hardware scenarios in specs/011-wifi-firmware-update/quickstart.md

**Checkpoint**: Interrupted work cannot strand the device; recovery behavior is observable
without source or serial-debug knowledge.

---

## Phase 6: User Story 4 - Understand Update Status and Outcome (Priority: P3)

**Goal**: The owner can distinguish uploading, validation, restart, success, failure, and
recovery states and can verify the installed identity.

**Independent Test**: Exercise both a successful and failed attempt and compare page/status
output at every stage.

- [ ] T026 [US4] Publish stable plain-language session states and percentages in wifi_config/wifi_firmware_update.c
- [ ] T027 [US4] Serialize safe `GET /update/status` JSON including build identity and next action in wifi_config/wifi_sta_http.c
- [ ] T028 [US4] Add final success/failure/recovery messaging and build identity presentation in wifi_config/wifi_sta_web.c
- [ ] T029 [US4] Validate the end-to-end status/outcome scenarios in specs/011-wifi-firmware-update/quickstart.md

**Checkpoint**: Status is never ambiguous once the device is reachable after an attempt.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T030 [P] Update FOTA migration, package, and USB-recovery instructions in specs/011-wifi-firmware-update/quickstart.md
- [ ] T031 Verify all FOTA/flash paths have bounded buffers, no display-path heap allocation, and no PIO/DMA timing changes in wifi_config/wifi_firmware_update.c
- [ ] T032 Run `ninja -C build`, host package tests, and all applicable quickstart scenarios; record artifact sizes in specs/011-wifi-firmware-update/quickstart.md

## Dependencies & Execution Order

- Phase 1 → Phase 2 → US1 → US2 → US3 → US4 → Phase 7.
- US2 shares the transport/session introduced by US1; US3 relies on the validated slot flow;
  US4 presents state from all prior stories.
- T003/T004 and T008 may run in parallel. All other tasks touching the same CMake, portal,
  or update-session files are sequential.

## Implementation Strategy

1. Establish repeatable bootloader/package outputs and flash partitioning.
2. Build and validate the bounded update module before accepting any upload.
3. Deliver US1 as the WiFi update MVP, then harden invalid inputs and recovery.
4. Finish status polish and execute the hardware acceptance guide.

All tasks use the required checklist format with unique IDs, story labels, and exact paths.
