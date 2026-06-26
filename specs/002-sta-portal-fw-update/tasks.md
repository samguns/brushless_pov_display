---
description: "Task list for STA Management Portal & Firmware Update"
---

# Tasks: STA Management Portal & Firmware Update

**Input**: Design documents from `specs/002-sta-portal-fw-update/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/http-api.md, quickstart.md

**Tests**: No automated test tasks are generated. The spec mandates manual hardware
validation only (no host-side unit-test framework exists for this RP2040 firmware).
Validation is via the linker `ASSERT` (build-time) and on-device serial checks
(see `quickstart.md`).

**Organization**: Tasks are grouped by user story to enable independent implementation and
testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- All paths are repository-relative to `/home/sam/CLionProjects/pov_leds`

## Path Conventions

Single embedded firmware project. New sources live under `wifi_config/`; the custom linker
script and build wiring live at the repository root (`memmap_wifi_creds.ld`, `CMakeLists.txt`,
`pov_leds.cpp`).

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Establish a clean build baseline before modifying linker/build configuration.

- [X] T001 Verify the current baseline builds cleanly with `ninja -C build` and note the
  firmware size (`__flash_binary_end` in `build/pov_leds.elf.map`) so later changes can be
  compared against the credential-region boundary (`0x101FF000`).

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: STA-mode serving scaffolding that User Stories 2 and 3 both build on. (User
Story 1 is independent of this phase and may proceed in parallel.)

**⚠️ CRITICAL**: US2 and US3 cannot begin until this phase is complete.

- [X] T002 Add `wifi_config_run_sta(void)` declaration to `wifi_config/wifi_config.h`
  (public entry point called by `main()` after `wifi_config_init()` returns / after a normal
  STA boot).
- [X] T003 Create the STA HTTP server skeleton in `wifi_config/wifi_sta_http.h` and
  `wifi_config/wifi_sta_http.c`: raw-lwIP single-client server on port 80 mirroring the
  `wifi_http.c` pattern (`on_accept`/`on_recv`/`on_client_err`, static request buffer,
  `send_response()`/`send_redirect()` helpers, `wifi_sta_http_start()`,
  `wifi_sta_http_stop()`, `wifi_sta_http_poll()`). Dispatch by method+path; unknown paths →
  302 redirect to `/`. Leave `GET /`, `GET /update`, `POST /update` handlers as stubs to be
  filled by US2/US3.
- [X] T004 Add the new sources `wifi_config/wifi_sta_http.c` and `wifi_config/wifi_sta_web.c`
  to `add_executable(pov_leds ...)` in `CMakeLists.txt` (create an empty
  `wifi_config/wifi_sta_web.c`/`.h` pair so the build links; bodies filled in US2/US3).
- [X] T005 Implement `wifi_config_run_sta()` in `wifi_config/wifi_config.c`: assumes STA is
  already connected, reads stored credentials via `load_credentials()` for the SSID, starts
  `wifi_sta_http_start()`, then loops calling `cyw43_arch_poll()` + `wifi_sta_http_poll()`.
  Include the link-status monitoring hook (filled in US2's FR-014 task) as a no-op for now.
- [X] T006 Wire `pov_leds.cpp` to call `wifi_config_run_sta()` after `wifi_config_init()`
  returns, before the existing PIO blink loop (so the STA portal serves while the device runs).

**Checkpoint**: Firmware builds and links with the STA server skeleton running in STA mode
(serving 302 redirects on every path). US2 and US3 can now begin.

---

## Phase 3: User Story 1 - WiFi Credentials Survive Firmware Update (Priority: P1) 🎯 MVP

**Goal**: Reserve the last 4 KB flash sector at link time so a UF2 firmware update cannot
overwrite stored credentials, with a build-time `ASSERT` if firmware would overflow.

**Independent Test**: Provision credentials, dump `0x101FF000`, flash a new UF2, dump again
— the region is byte-identical and the device auto-connects without AP mode (quickstart
Scenario 1). Independent of Phases 2/4/5.

### Implementation for User Story 1

- [X] T007 [US1] Create `memmap_wifi_creds.ld` at the repository root as a derivative of the
  SDK `memmap_default.ld`: replace the `INCLUDE "pico_flash_region.ld"` line with an explicit
  `FLASH(rx) : ORIGIN = 0x10000000, LENGTH = (2 * 1024 * 1024) - 4096` so the last 4 KB sector
  is not linkable. Keep all other regions/sections identical to the SDK default.
- [X] T008 [US1] In `memmap_wifi_creds.ld`, add after the `.flash_end`/`__flash_binary_end`
  section an `ASSERT(__flash_binary_end <= ORIGIN(FLASH) + LENGTH(FLASH), "ERROR: firmware
  overflows reserved WiFi credential sector (last 4 KB)")` to satisfy FR-003 / SC-005.
- [X] T009 [US1] Wire the custom script into the build by adding
  `pico_set_linker_script(pov_leds ${CMAKE_CURRENT_LIST_DIR}/memmap_wifi_creds.ld)` to
  `CMakeLists.txt`, then rebuild with `ninja -C build` and confirm the link succeeds and the
  credential offset used by `wifi_config/wifi_flash.c` (`PICO_FLASH_SIZE_BYTES - 4096`) is
  unchanged.
- [X] T010 [US1] Verify FR-004 forward compatibility by confirming `wifi_flash.c` still reads
  `0x101FF000` (no offset/format change) and document the protected boundary in a comment in
  `memmap_wifi_creds.ld` referencing `specs/002-sta-portal-fw-update/data-model.md`.

**Checkpoint**: Build enforces the credential boundary; credentials survive UF2 updates.

---

## Phase 4: User Story 2 - STA Management Page with IP Address (Priority: P1)

**Goal**: While in STA mode, print the IP/SSID to serial and serve a management page showing
the device IP and connected SSID with an "Update firmware" button.

**Independent Test**: Boot a provisioned device, read IP/SSID from serial, browse to
`http://<IP>/`, confirm the page shows IP + SSID + an Update button (quickstart Scenario 2).
Depends on Phase 2.

### Implementation for User Story 2

- [X] T011 [US2] In `wifi_config/wifi_config.c` `wifi_config_run_sta()`, read the live IPv4
  address from the STA netif (`netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA])`) and print
  the connected SSID + IP to USB serial within 500 ms of the lease (FR-005, SC-002), e.g.
  `[wifi_config] STA connected — SSID: %s  IP: %s`.
- [X] T012 [P] [US2] Implement `wifi_sta_web_build_status_page(char *buf, size_t buflen,
  const char *ssid, const char *ip)` in `wifi_config/wifi_sta_web.c` (+ declaration in
  `wifi_sta_web.h`) rendering the status page from contracts/http-api.md `GET /`: shows IP,
  SSID, and an "Update firmware" link to `/update` (FR-006, FR-007, FR-008). Returns bytes
  written or -1.
- [X] T013 [US2] Implement the `GET /` handler in `wifi_config/wifi_sta_http.c` to render the
  status page via `wifi_sta_web_build_status_page()` using the IP/SSID captured by
  `wifi_config_run_sta()` (pass them to the server via `wifi_sta_http_start(ssid, ip)` or a
  setter), responding `200 OK` within 3 s (SC-003).
- [X] T014 [US2] Implement WiFi-drop handling (FR-014) in `wifi_config_run_sta()`: poll
  `cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA)`; on link loss attempt silent
  reconnect with stored credentials (reuse the `try_sta()` pattern), and if not restored
  within 15 s, stop the STA server and fall back to AP provisioning
  (`WIFI_ERR_RECOVERY` path).

**Checkpoint**: Management page reachable in STA mode showing IP + SSID; drop fallback works.

---

## Phase 5: User Story 3 - Trigger Firmware Update Mode (Priority: P2)

**Goal**: From the management page, confirm a firmware update and reboot the device into USB
mass-storage (BOOTSEL) mode.

**Independent Test**: From the status page click "Update firmware" → confirmation page with
Confirm/Cancel + 60 s countdown; Confirm → `RPI-RP2` drive mounts within 3 s; Cancel → back
to status page (quickstart Scenario 3). Depends on Phases 2 and 4 (status page links here).

### Implementation for User Story 3

- [X] T015 [P] [US3] Implement `wifi_sta_web_build_update_page(char *buf, size_t buflen)` in
  `wifi_config/wifi_sta_web.c` (+ declaration in `wifi_sta_web.h`) rendering the confirmation
  page from contracts/http-api.md `GET /update`: warning text, a `POST /update` "Confirm
  update" form, a "Cancel" link to `/`, and a client-side 60 s countdown that redirects to
  `/` at 0 (FR-009, FR-011, FR-013).
- [X] T016 [US3] Implement `wifi_sta_web_build_rebooting_page(char *buf, size_t buflen)` in
  `wifi_config/wifi_sta_web.c` (+ declaration in `wifi_sta_web.h`): a brief "Rebooting into
  update mode…" page sent in response to `POST /update`.
- [X] T017 [US3] Implement the `GET /update` handler in `wifi_config/wifi_sta_http.c` to serve
  the confirmation page via `wifi_sta_web_build_update_page()`.
- [X] T018 [US3] Implement the `POST /update` handler in `wifi_config/wifi_sta_http.c`: send
  the rebooting page, poll lwIP ~600 ms to flush + close the connection, then call
  `reset_usb_boot(0, 0)` (`#include "pico/bootrom.h"`) to enter USB MSD within 3 s of confirm
  (FR-010, SC-004). Handle a second concurrent request via the existing single-client abort.

**Checkpoint**: Confirmed update reboots into USB MSD; Cancel/timeout leave STA mode intact.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final verification and documentation.

- [X] T019 Rebuild with `ninja -C build` and confirm a clean link with the custom linker
  script and all new sources; confirm `__flash_binary_end` remains below `0x101FF000` in
  `build/pov_leds.elf.map`.
- [X] T020 [P] Update the managed Spec Kit context note (`AGENTS.md`) is already pointed at
  this plan; confirm no stale references and that `wifi_config/` module comments reference
  feature 002 where new behaviour was added.
- [ ] T021 Execute `specs/002-sta-portal-fw-update/quickstart.md` Scenarios 1–4 on hardware
  and record pass/fail against SC-001…SC-006.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: Depends on Setup. BLOCKS User Stories 2 and 3.
- **User Story 1 (Phase 3)**: Independent — depends only on Setup (linker/build only). Can run
  fully in parallel with Phases 2/4/5.
- **User Story 2 (Phase 4)**: Depends on Foundational (Phase 2).
- **User Story 3 (Phase 5)**: Depends on Foundational (Phase 2) and on US2's status page
  (the "Update firmware" link target) — best done after Phase 4.
- **Polish (Phase 6)**: Depends on all desired stories being complete.

### User Story Dependencies

- **US1 (P1)**: Fully independent (build-time only).
- **US2 (P1)**: Needs the Phase 2 STA server skeleton.
- **US3 (P2)**: Needs Phase 2 + the US2 status page to link to.

### Within Each User Story

- Web page builders (`wifi_sta_web.c`) before the HTTP handlers that call them.
- HTTP handlers before end-to-end serial/browser validation.

### Parallel Opportunities

- US1 (Phase 3) can run entirely in parallel with Phases 2/4/5 (different files:
  `memmap_wifi_creds.ld` + `CMakeLists.txt` linker line vs. `wifi_config/` sources).
- Within US2: T012 (`wifi_sta_web.c` status page) is [P] vs. T011 (serial/IP in
  `wifi_config.c`).
- Within US3: T015 and T016 (both `wifi_sta_web.c` page builders) — same file, so author
  sequentially, but independent of T017/T018 handlers conceptually.

---

## Parallel Example: User Story 2

```bash
# Independent files can progress together:
Task: "T011 [US2] Print STA IP/SSID to serial in wifi_config/wifi_config.c"
Task: "T012 [US2] Build status page in wifi_config/wifi_sta_web.c"
```

---

## Implementation Strategy

### MVP First (User Stories 1 + 2)

Both US1 and US2 are P1. The MVP is: credentials survive updates (US1) **and** the device is
reachable/observable in STA mode (US2).

1. Phase 1 Setup → baseline build.
2. Phase 3 (US1) linker reservation — independent, deliver immediately.
3. Phase 2 Foundational → Phase 4 (US2) management page.
4. **STOP and VALIDATE**: quickstart Scenarios 1 and 2.

### Incremental Delivery

1. US1 (build-time credential protection) → validate Scenario 1.
2. US2 (STA portal + IP) → validate Scenario 2.
3. US3 (firmware update trigger) → validate Scenario 3.
4. WiFi-drop fallback (T014) → validate Scenario 4.

---

## Notes

- [P] tasks = different files, no dependencies.
- Reuse the proven raw-lwIP pattern from `wifi_config/wifi_http.c`; do not introduce lwIP
  httpd/CGI.
- The AP-mode server is stopped before STA serving; no port-80 conflict.
- `reset_usb_boot()` does not return — always send + flush the HTTP response first.
- Do not change the credential offset/format (`PICO_FLASH_SIZE_BYTES - 4096`); forward
  compatibility (FR-004) depends on it.
- Commit after each task or logical group.
