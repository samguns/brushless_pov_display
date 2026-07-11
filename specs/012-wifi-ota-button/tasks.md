# Tasks: Update Firmware (WIFI) Button

## Phase 1: Setup

- [X] T001 Verify the existing FOTA session API and package artifacts in wifi_config/wifi_firmware_update.{c,h}

## Phase 2: Foundational

- [X] T002 Implement bounded HTTP body streaming and upload abort handling in wifi_config/wifi_sta_http.c
- [ ] T003 Add authenticated OTA upload/status route declarations in wifi_config/wifi_sta_http.h

## Phase 3: User Story 1 - Choose an Update Method (P1)

**Goal**: Show separate USB and WiFi routes without regressing USB recovery.

- [X] T004 [US1] Add distinct Update Firmware (USB) and WiFi OTA actions to the System card in wifi_config/wifi_sta_web.c
- [X] T005 [US1] Add a dedicated WiFi OTA page builder with package and recovery guidance in wifi_config/wifi_sta_web.{c,h}
- [X] T006 [US1] Route `GET /ota` while retaining `GET/POST /update` USB behavior in wifi_config/wifi_sta_http.c

## Phase 4: User Story 2 - Upload a Firmware Package (P1)

- [X] T007 [US2] Stream one authorized browser upload into the FOTA session in wifi_config/wifi_sta_http.c
- [X] T008 [US2] Render upload progress, duplicate prevention, validation, and restart state in wifi_config/wifi_sta_web.c
- [X] T009 [US2] Expose safe OTA session status and installed build identity in wifi_config/wifi_sta_http.c

## Phase 5: User Story 3 - Safe Failure Guidance (P2)

- [X] T010 [US3] Preserve upload failure state and USB recovery guidance in wifi_config/wifi_sta_web.c
- [ ] T011 [US3] Validate USB, valid OTA, invalid OTA, interruption, and duplicate-submission scenarios in specs/012-wifi-ota-button/quickstart.md

## Phase 6: Polish

- [X] T012 Run `cmake --build build` and host package tests, then mark validated tasks in specs/012-wifi-ota-button/tasks.md

## Phase 7: Label & Progress Refinement

- [X] T013 [US1] Rename the browser-upload action to Update Firmware (WIFI) in wifi_config/wifi_sta_web.c
- [X] T014 [US2] Add percentage, progress bar, upload stage, and recovery-focused visual hierarchy in wifi_config/wifi_sta_web.c
- [X] T015 Update the refined validation scenarios in specs/012-wifi-ota-button/quickstart.md

## Dependencies

T001–T003 block all stories. US1 precedes US2; US3 follows upload/status behavior.
