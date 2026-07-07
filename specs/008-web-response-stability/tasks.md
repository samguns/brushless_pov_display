# Tasks: Web Response Stability

**Input**: Design documents from `specs/008-web-response-stability/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Build validation plus manual device/browser scenarios from quickstart.md.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Verify the feature context and project hygiene before code changes.

- [x] T001 Verify C/C++ ignore patterns in `.gitignore`
- [x] T002 [P] Review `wifi_config/wifi_sta_http.c` client lifecycle state against `specs/008-web-response-stability/research.md`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Add shared cleanup and progress primitives used by all reliability stories.

- [x] T003 Add centralized client state cleanup helper in `wifi_config/wifi_sta_http.c`
- [x] T004 Add per-client progress timestamp and timeout constants in `wifi_config/wifi_sta_http.c`

---

## Phase 3: User Story 1 - Portal responds after idle browser sockets (Priority: P1) MVP

**Goal**: Release silent browser/preconnect sockets so later real requests succeed.

**Independent Test**: Connect without sending data, wait past the idle window, then request `GET /` successfully.

- [x] T005 [US1] Install an lwIP poll callback for accepted clients in `wifi_config/wifi_sta_http.c`
- [x] T006 [US1] Abort silent idle clients from the poll callback in `wifi_config/wifi_sta_http.c`

**Checkpoint**: Silent clients no longer permanently occupy the portal slot.

---

## Phase 4: User Story 2 - Portal recovers from stalled transfers (Priority: P2)

**Goal**: Release clients whose response streaming stops making progress.

**Independent Test**: Stall a response transfer, wait past the progress window, then request `GET /status` successfully.

- [x] T007 [US2] Refresh progress on receive, queued send chunks, and sent acknowledgments in `wifi_config/wifi_sta_http.c`
- [x] T008 [US2] Close stalled active transfers from the poll callback in `wifi_config/wifi_sta_http.c`

**Checkpoint**: Stalled transfers are bounded and later requests can connect.

---

## Phase 5: User Story 3 - Portal remains safe for existing actions (Priority: P3)

**Goal**: Preserve existing routes and deferred work semantics.

**Independent Test**: Build the firmware and exercise existing portal routes.

- [x] T009 [US3] Ensure normal send finish, redirects, errors, stop, and reboot flushing clear or preserve client state correctly in `wifi_config/wifi_sta_http.c`
- [x] T010 [US3] Run `ninja -C build` and record the result

**Checkpoint**: Existing portal workflows remain compatible.

---

## Dependencies & Execution Order

- Phase 1 precedes all implementation.
- Phase 2 blocks all user stories.
- US1 is the MVP and should be completed before US2.
- US3 validates compatibility after US1 and US2.

## Parallel Opportunities

- T002 can run in parallel with T001.
- User-story implementation touches the same source file, so T003 through T009 should run sequentially.

## Implementation Strategy

1. Complete setup and foundational helper work.
2. Implement the idle-client MVP.
3. Extend progress tracking to response streaming.
4. Validate with the build and the quickstart scenarios.

## Validation Notes

- Build passed with `C:\Users\eggy2\.pico-sdk\ninja\v1.12.1\ninja.exe -C build`.
- `ninja` and `cmake` are not on PATH in this shell. The generated build also failed to find `lwipopts.h` through its absolute Windows include path, so the existing `lwipopts.h` was copied to ignored `build/lwipopts.h` for local verification.
