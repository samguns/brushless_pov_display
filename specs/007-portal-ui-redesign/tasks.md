---

description: "Task list for Management Portal UI/UX Redesign (007-portal-ui-redesign)"
---

# Tasks: Management Portal UI/UX Redesign

**Input**: Design documents from `specs/007-portal-ui-redesign/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/http-api.md, quickstart.md

**Tests**: Not requested — the spec validates via manual hardware/browser testing
(quickstart.md). No automated test tasks are generated.

**Organization**: Tasks are grouped by user story. Because the portal is
server-rendered as C string builders, most page work lives in
`wifi_config/wifi_sta_web.c` (one file) and is therefore sequential (not `[P]`)
even when conceptually independent. `[P]` is used only for tasks in different
files with no incomplete dependencies.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1–US5 map to the spec's user stories
- File paths are repository-relative.

## Path note

All paths are relative to repo root `/home/sam/CLionProjects/pov_leds/`.
Key files: `wifi_config/wifi_sta_web.{c,h}` (pages), `wifi_config/wifi_sta_http.c`
(routing/buffer), `wifi_config/wifi_config.{c,h}` (runtime brightness),
`wifi_config/wifi_flash.{c,h}` (persistence), `ws2812_driver.{h,cpp}` (LED
brightness), `pov_leds.cpp` (apply brightness).

---

## Phase 1: Setup (Shared Prep)

**Purpose**: Capture exact design values before authoring CSS.

- [X] T001 [P] Extract exact design tokens from the Figma frames (Overview `1:2`, settings-screen `9:4`) using the Figma `get_design_context`/`get_variable_defs` tools — colors (background, card bg, card border, muted label, accent teal, destructive red), spacing, border-radius, font sizes/weights, and the monospace usage for values — and record them as a `## Design Tokens` table appended to `specs/007-portal-ui-redesign/research.md` to drive the CSS.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared UI shell + memory budget that every screen depends on.

**⚠️ CRITICAL**: No user story screen work can begin until this phase is complete.

- [X] T002 Enlarge the static page buffer `STA_PAGE_BUF_SIZE` from 8192 to 16384 in `wifi_config/wifi_sta_http.c`, and confirm the overflow guards remain intact (page builders return `-1` and the handler sends an error/short response rather than truncating).
- [X] T003 Add the shared design-system CSS block (the `STA_STYLE` macro) in `wifi_config/wifi_sta_web.c`: `:root` dark-theme custom properties + `[data-theme="light"]` overrides, system sans + monospace font stacks, layout primitives (app grid, sidebar, header, main), `.card`/label/value, buttons (incl. `.destructive`), banner/notice, form inputs, toggle + range-slider styles, read-only/disabled field styles, and a responsive breakpoint (≤ ~640px stacks the sidebar and cards). Use the tokens from T001. (depends on T001)
- [X] T004 Add the shared theme-apply inline `<script>` snippet (reads `localStorage`, applies `data-theme` on load, default dark) as a reusable string/macro in `wifi_config/wifi_sta_web.c`, to be included by every page. (depends on T003)
- [X] T005 Add shared shell builder helpers in `wifi_config/wifi_sta_web.c`: a head/open helper (doctype, `<head>`, `STA_STYLE`, theme JS), a `sta_append_sidebar(active)` helper (POV Display brand + inline-SVG icon, Overview/Settings nav with active-state highlight, decorative profile footer), a decorative header helper (avatar/notification), and a page-close helper — all reused by Overview and Settings. (depends on T003, T004)

**Checkpoint**: Shared dark/light shell, theme persistence, and page buffer ready.

---

## Phase 3: User Story 1 - Modern Overview dashboard (Priority: P1) 🎯 MVP

**Goal**: Re-skin the status page into the Figma Overview screen with sidebar +
metric cards showing real device state.

**Independent Test**: Load `http://<device-ip>/` and confirm the dark Overview
with sidebar and Status / Network (SSID) / IP Address / blink cards matches Figma
`1:2` and shows real values (quickstart Scenario A).

- [X] T006 [US1] Rewrite `wifi_sta_web_build_status_page` in `wifi_config/wifi_sta_web.c` to render the Overview screen via the shared shell: page title "Overview" + date line, metric cards for Status (connectivity), Network (SSID), IP Address, and blink active/frequency, plus the styled notice banner — matching Figma `1:2`. (depends on Phase 2)
- [X] T007 [US1] In `wifi_config/wifi_sta_http.c`, confirm `GET /` routes to the Overview builder and passes real values (state/ssid/ip/blink/notice); ensure the sidebar's Settings link targets `/settings`. (depends on T006)

**Checkpoint**: Overview screen is the demonstrable MVP.

---

## Phase 4: User Story 2 - Redesigned Settings: change Wi-Fi (Priority: P2)

**Goal**: Build the Settings screen with the Network card so the owner changes
Wi-Fi from the redesigned UI, preserving existing behavior.

**Independent Test**: Open `/settings`, Scan, select/enter SSID + password,
submit, and see the re-skinned applying flow; invalid input shows an inline error
(quickstart Scenario E).

- [X] T008 [US2] Add the Settings page assembler `wifi_sta_web_build_settings_page(...)` in `wifi_config/wifi_sta_web.c` (shared shell + a cards grid) initially rendering the **Network card**: current SSID, selectable scan-result list (select-to-fill), masked always-empty password, Connect submit to `POST /config`, and the **read-only** Static IP toggle + IP/Subnet/Gateway fields — matching Figma `9:4`. Supersedes `wifi_sta_web_build_wifi_page`. (depends on Phase 2)
- [X] T009 [US2] Update `wifi_config/wifi_sta_web.h`: declare the new settings-page builder (params: ssid, ip, scan results, n_results, brightness, fw_version, notice) and adjust/remove the old `wifi_sta_web_build_wifi_page` declaration accordingly. (depends on T008)
- [X] T010 [US2] Re-skin `wifi_sta_web_build_applying_page` in `wifi_config/wifi_sta_web.c` to the new visual style (preserve the reconnect guidance). (depends on Phase 2)
- [X] T011 [US2] Update routing in `wifi_config/wifi_sta_http.c`: serve the Settings screen at `GET /settings` and `GET /settings?scan=1`, keep `GET /wifi` as an alias, and call the new assembler with real ssid/ip/scan-results (and placeholders for brightness/fw until US3/US5); keep `POST /config` validate→stage→apply behavior unchanged. (depends on T008, T009)

**Checkpoint**: Overview + Settings (Network) both work; Wi-Fi change unchanged behaviorally.

---

## Phase 5: User Story 3 - Redesigned Settings: firmware update (Priority: P3)

**Goal**: Add the System card (firmware version + Update Firmware) to Settings
and re-skin the firmware pages.

**Independent Test**: On `/settings`, the System card shows the version and a
prominent Update Firmware action leading to the re-skinned confirm page
(quickstart Scenario F).

- [X] T012 [P] [US3] Expose a firmware version string constant (from the program version, e.g. `WIFI_STA_FW_VERSION "0.1"`) for the System card — define in `wifi_config/wifi_sta_web.h` (or a small shared header). 
- [X] T013 [US3] Add the **System card** section to the Settings assembler in `wifi_config/wifi_sta_web.c`: firmware version (read-only) + prominent/destructive **Update Firmware** action linking to `GET /update`. (depends on T008, T012)
- [X] T014 [US3] Re-skin `wifi_sta_web_build_update_page` and `wifi_sta_web_build_rebooting_page` in `wifi_config/wifi_sta_web.c` to the new style, keeping the 60 s countdown, Confirm/Cancel, and reboot behavior. (depends on Phase 2)
- [X] T015 [US3] In `wifi_config/wifi_sta_http.c`, pass `WIFI_STA_FW_VERSION` into the Settings assembler and verify the System card's Update Firmware reaches the existing `GET /update` page and that `POST /update` (reboot to USB MSD) is unchanged. (depends on T013)

**Checkpoint**: Settings shows System card; firmware flow re-skinned, behavior intact.

---

## Phase 6: User Story 5 - Display preferences: theme & brightness (Priority: P3)

**Goal**: Add the Display card with a working Dark/Light theme toggle and a
brightness control that drives the LED panel and persists across reboot.

**Independent Test**: Toggle theme (persists across reload); set brightness and
see the panel dim/brighten; power-cycle and confirm it is restored (quickstart
Scenarios C & D).

- [X] T016 [P] [US5] Add a global brightness scalar to the WS2812 driver: a `brightness` field + `ws2812_driver_set_brightness(ws2812_driver_t*, uint8_t)` in `ws2812_driver.h`/`ws2812_driver.cpp`, and scale each GRB byte in `ws2812_driver_submit_frame` (e.g. `v = v * brightness / 255`); default full; do not alter the PIO program or bit timing.
- [X] T017 [P] [US5] Extend flash persistence to V3 in `wifi_config/wifi_flash.c` and `wifi_config/wifi_flash.h`: add `wifi_flash_record_v3_t` (adds `uint8_t brightness`; `WIFI_FLASH_MAGIC_V3`/`WIFI_FLASH_VERSION_V3`), update `load_credentials` to accept V3→V2→V1 (default brightness for older records), add `save_brightness(uint8_t)` (read-modify-write preserving SSID/password/admin_token via the existing atomic erase+write+verify) and a settings-load accessor, and update the `_Static_assert` size checks.
- [X] T018 [US5] Add runtime brightness to `wifi_config/wifi_config.c` + `wifi_config/wifi_config.h`: store brightness in `wifi_runtime_state_t`, load it at `wifi_config_sta_runtime_init` (from T017), and add `wifi_config_get_brightness()` / `wifi_config_set_brightness(uint8_t)` (clamp, apply to runtime, persist via `save_brightness` only when changed). (depends on T017)
- [X] T019 [US5] In `pov_leds.cpp`, apply the persisted brightness after WS2812 init via `ws2812_driver_set_brightness(wifi_config_get_brightness())`, and keep it in sync when it changes (re-read each loop or on change). (depends on T016, T018)
- [X] T020 [US5] Add the **Display card** section to the Settings assembler in `wifi_config/wifi_sta_web.c`: a Dark/Light theme toggle (using the shared theme JS from T004) and a brightness range control (0–100) initialized to the current device brightness, submitting to `POST /display` — matching Figma `9:4`. (depends on T008)
- [X] T021 [US5] Add the `POST /display` route in `wifi_config/wifi_sta_http.c`: parse + clamp `brightness` (0–100), call `wifi_config_set_brightness`, then re-render Settings (or redirect to `/settings`) with a brief confirmation; log received/applied/persisted-or-skipped (no secrets). (depends on T018, T020)

**Checkpoint**: Theme toggle works; brightness changes the LEDs and survives reboot.

---

## Phase 7: User Story 4 - Consistent navigation & responsive layout (Priority: P4)

**Goal**: Ensure the shared nav shell and responsive behavior are correct across
all screens.

**Independent Test**: On desktop and phone widths, navigate Overview↔Settings on
every screen; content reflows with no horizontal scroll (quickstart Scenario H).

- [X] T022 [US4] Verify the shared sidebar nav (Overview/Settings with correct active state and links) renders on every page — Overview, Settings, applying, update, rebooting — in `wifi_config/wifi_sta_web.c`, fixing any page that bypasses the shared shell.
- [X] T023 [US4] Verify and tune the responsive breakpoint in the shared CSS (`wifi_config/wifi_sta_web.c`) so the sidebar collapses/stacks and cards reflow to a single column, fully usable at ≤ 420 px with no horizontal scrolling.

**Checkpoint**: Cohesive navigation and mobile layout across all screens.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [X] T024 [P] Build with `ninja -C build` and resolve any warnings; confirm the `wifi_flash_record_v3_t` static-size assert passes and the cold-clone single-command build holds (Principle V).
- [X] T025 Verify the largest page (Settings with a full 20-network scan list) returns a `Content-Length` comfortably below `STA_PAGE_BUF_SIZE` (16384) with no truncation; adjust the buffer in `wifi_config/wifi_sta_http.c` if needed and re-verify (FR-014/SC-007).
- [ ] T026 [P] Verify no heap was introduced and that brightness persists write-on-change (flash write skipped when unchanged) via serial logs (Principle IV). [Code path implements write-on-change in `wifi_config_set_brightness`; runtime serial-log confirmation pending on hardware.]
- [X] T027 Confirm `CMakeLists.txt` still lists all sources and that no new source files were added (all edits are in existing files); update only if a new file was introduced.
- [ ] T028 Run all quickstart.md validation scenarios (A–H) on hardware at desktop and phone widths, comparing rendered Overview/Settings against Figma frames `1:2` and `9:4` (SC-001, SC-005, SC-011). [Requires physical device; pending owner validation.]

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: no dependencies.
- **Foundational (Phase 2)**: depends on Setup; **blocks all user stories**.
- **US1 (Phase 3)**: depends on Foundational. MVP.
- **US2 (Phase 4)**: depends on Foundational. Creates the Settings assembler that US3 and US5 extend.
- **US3 (Phase 5)** and **US5 (Phase 6)**: depend on Foundational and on the Settings assembler (T008 in US2).
- **US4 (Phase 7)**: depends on the screens existing (US1, US2; ideally after US3/US5 so all pages are present).
- **Polish (Phase 8)**: after the desired stories are complete.

### Story-level notes

- US1 is fully independent once Foundational is done.
- US2 introduces `wifi_sta_web_build_settings_page` (T008); US3 (T013) and US5 (T020) add card sections to that same assembler, so they build on T008 but each renders an independently verifiable Settings increment.
- The brightness backend (US5: T016 driver, T017 flash) is independent of the page work and can proceed in parallel with the CSS/page tasks.

### Within a story

- Backend before the route that uses it (T017→T018→T019; T018→T021).
- Assembler (T008) before card sections (T013, T020).

---

## Parallel Opportunities

Tasks in **different files** with no incomplete dependencies can run together:

- **T001** (Figma token extraction → research.md) can start immediately.
- After Foundational, the US5 backend is parallelizable across files:

```bash
# Different files, no shared deps:
Task: "T016 [US5] WS2812 driver brightness scalar in ws2812_driver.{h,cpp}"
Task: "T017 [US5] Flash V3 record + save_brightness in wifi_config/wifi_flash.{c,h}"
```

- **T012** (firmware version constant) is `[P]` vs the page work.
- Note: all tasks editing `wifi_config/wifi_sta_web.c` (T003, T004, T005, T006, T008, T010, T013, T014, T020, T022, T023) are **sequential** — same file.

---

## Implementation Strategy

### MVP first

1. Phase 1 (Setup) → Phase 2 (Foundational shell) → Phase 3 (US1 Overview).
2. **STOP and VALIDATE** the Overview against Figma `1:2`. Demo the MVP.

### Incremental delivery

1. Foundation ready → US1 Overview (MVP, Scenario A).
2. US2 Settings/Network → Wi-Fi change works in new UI (Scenario E).
3. US3 System card → firmware flow re-skinned (Scenario F).
4. US5 Display card + brightness backend → theme + LED brightness (Scenarios C, D).
5. US4 → cross-screen nav + responsive polish (Scenario H).
6. Phase 8 → build/budget/regression sweep + full quickstart.

---

## Notes

- `[P]` = different files, no incomplete dependencies.
- Preserve all existing behavior/validation/security for status, scan, Wi-Fi
  change, and firmware update (FR-013) — only presentation changes, plus the new
  `POST /display` and brightness backend.
- Keep pages self-contained (no external assets), brightness scaling out of the
  PIO timing path, and all allocation static (Constitution I, II, IV).
- Commit after each task or logical group; validate at each checkpoint.
