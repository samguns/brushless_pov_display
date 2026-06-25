# Tasks: WiFi Configuration Web App

**Input**: Design documents from `specs/001-wifi-config-webapp/`

**Prerequisites**: plan.md ✅ · spec.md ✅ · research.md ✅ · data-model.md ✅ · contracts/http-api.md ✅ · quickstart.md ✅

---

## Format: `- [ ] [ID] [P?] [Story?] Description — file path`

- **[P]**: Can run in parallel with other [P] tasks in the same phase (operates on different files)
- **[US#]**: User story label (US1–US4 map to spec.md user stories)
- No story label = Setup or Foundational phase task

---

## Phase 1: Setup

**Purpose**: Create the module directory, wire the build system, and establish the lwIP configuration that all subsequent code depends on.

- [X] T001 Create `wifi_config/` directory and add all its planned source files as empty stubs to `CMakeLists.txt` via `target_sources(pov_leds PRIVATE ...)` — `CMakeLists.txt`
- [X] T002 Add required Pico SDK libraries to `target_link_libraries` in CMakeLists.txt: `pico_cyw43_arch_lwip_poll`, `pico_lwip_http`, `pico_lwip_dns` — `CMakeLists.txt`
- [X] T003 [P] Create `lwipopts.h` at the repository root with lwIP overrides: enable httpd CGI (`LWIP_HTTPD_CGI 1`), SSI (`LWIP_HTTPD_SSI 1`), DHCP server (`LWIP_DHCP 1`), DNS server (`LWIP_DNS 1`), set `MEM_SIZE`, `MEMP_NUM_TCP_SEG`, and `TCP_MSS` to values appropriate for a single-connection embedded HTTP server — `lwipopts.h`
- [X] T004 [P] Create `wifi_config/wifi_config.h` — public API header declaring `wifi_config_init()`, `wifi_config_run()`, and the `wifi_credentials_t` struct (`char ssid[33]`, `char password[64]`) — `wifi_config/wifi_config.h`

**Checkpoint**: Build system is wired; `ninja -C build` compiles (stubs produce linker errors only).

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Flash credential persistence layer — required by every user story.

**⚠️ CRITICAL**: US1 save flow, US2 boot read, US3 recovery all depend on this phase being complete.

- [X] T005 Implement `wifi_flash.h` — declare `load_credentials(wifi_credentials_t *out)` (returns bool), `bool save_credentials(const wifi_credentials_t *creds)` (returns true on success, false if flash erase or write fails), `clear_credentials()`, and the flash layout constants (`WIFI_FLASH_OFFSET = PICO_FLASH_SIZE_BYTES - 4096`, magic `0xC0FFEE01`) — `wifi_config/wifi_flash.h`
- [X] T006 Implement `wifi_flash.c` — `load_credentials()`: read from XIP address, check magic, compute CRC-32 over bytes 0–101, validate SSID non-empty; `save_credentials()`: erase sector, write struct with magic and CRC, run from RAM using `__no_inline_not_in_flash_func`, return false if `flash_range_erase` or `flash_range_program` cannot be verified (re-read and compare), return true on success; `clear_credentials()`: erase sector — `wifi_config/wifi_flash.c`
- [X] T007 Add `wifi_config_init()` call to `main()` in `pov_leds.cpp`, before the PIO/LED loop — `pov_leds.cpp`

**Checkpoint**: Flash read/write compiles; `load_credentials()` on erased device returns `false`.

---

## Phase 3: User Story 1 — First-Time Setup via Captive Portal (Priority: P1) 🎯 MVP

**Goal**: Device with no stored credentials boots into WPA2 AP, serves a scan-and-select config page, validates the user's chosen network connection, saves credentials, and reboots into STA mode.

**Independent test**: See [quickstart.md Scenario 1](quickstart.md#scenario-1-first-time-setup-no-credentials-stored).

- [X] T008 [P] [US1] Implement `wifi_scan.h` — declare `wifi_scan_start()`, `wifi_scan_is_active()`, `wifi_scan_get_results(scan_result_t *out, int max)`, and `scan_result_t` struct (`char ssid[33]`, `int16_t rssi`, `uint8_t secured`) — `wifi_config/wifi_scan.h`
- [X] T009 [P] [US1] Implement `wifi_scan.c` — call `cyw43_wifi_scan()` with a callback that deduplicates entries by SSID (keep strongest RSSI), stores up to 20 results in a static array, sorts descending by RSSI when scan completes; `wifi_scan_is_active()` wraps `cyw43_wifi_scan_active()` — `wifi_config/wifi_scan.c`
- [X] T010 [P] [US1] Create `wifi_web.h` — embed three HTML strings as C `const char[]` constants: (1) the configuration page template with `<!--#networks-->` and `<!--#scan_count-->` SSI slots, a `<select name="ssid">` element populated by scan results, and an always-visible plain text field labelled "Or enter network name manually:" (`name="ssid_manual"`, `maxlength="32"`) for hidden networks — no JavaScript required; (2) the "Connecting…" response page with self-contained instructional text (AP will disappear on success, reappear on failure); (3) an error banner snippet for injection into the config page — `wifi_config/wifi_web.h`
- [X] T011 [US1] Implement `wifi_http.h/wifi_http.c` — register CGI handler for `POST /connect` and SSI handler for `GET /`; SSI handler: call `wifi_scan_start()`, wait for scan, render `<option>` elements from scan results into `<!--#networks-->` slot and count into `<!--#scan_count-->`; CGI handler: parse URL-encoded body (extract `ssid`, `ssid_manual`, `password`), resolve effective SSID (use `ssid` if non-empty, else use `ssid_manual` if non-empty, else reject with 400), validate effective SSID 1–32 chars and password ≤63 chars, respond with 400 error page on invalid input, respond with Connecting page on valid input and set a module-level `pending_connect` flag with the credentials — `wifi_config/wifi_http.h`, `wifi_config/wifi_http.c`
- [X] T012 [P] [US1] Implement `wifi_dns.h/wifi_dns.c` — UDP port 53 listener using lwIP raw UDP API; answer all A-record queries with `192.168.4.1`; start/stop functions for use from AP mode setup — `wifi_config/wifi_dns.h`, `wifi_config/wifi_dns.c`
- [X] T013 [US1] Implement `wifi_config_start_ap()` in `wifi_config.c` — call `cyw43_arch_init()`, `cyw43_arch_enable_ap_mode("pov-leds-setup", "12345678", CYW43_AUTH_WPA2_AES_PSK)`, enable lwIP DHCP server on the AP netif, call `wifi_http_start()`, call `wifi_dns_start()`, then run a `cyw43_arch_poll()` loop — `wifi_config/wifi_config.c`
- [X] T014 [US1] Implement `wifi_config_connect_sta()` in `wifi_config.c` — stop DNS and HTTP, `cyw43_arch_disable_ap_mode()`, `cyw43_arch_enable_sta_mode()`, attempt connection with 15 000 ms timeout using `cyw43_arch_wifi_connect_timeout_ms()`; on `PICO_OK`: call `save_credentials()`; if `save_credentials()` returns false set `error_reason=SAVE_FAILED` and restart AP; if it returns true call `watchdog_reboot(0,0,0)`; on connection failure: set `error_reason` (auth vs. timeout), return to caller — `wifi_config/wifi_config.c`
- [X] T015 [US1] Implement `wifi_config_init()` boot decision in `wifi_config.c` — call `load_credentials()`: if false → `wifi_config_start_ap(error_reason=NONE)`; in the AP main loop, when `pending_connect` is set → call `wifi_config_connect_sta()` → if fails → restart AP with `error_reason` set — `wifi_config/wifi_config.c`

**Checkpoint**: Flash device with no credentials; AP `pov-leds-setup` appears; browser opens config page; scan list is populated; submitting correct credentials causes AP to drop and device to reboot.

---

## Phase 4: User Story 2 — Automatic Connection on Boot (Priority: P1)

**Goal**: Device with valid stored credentials connects in STA mode on boot with no user interaction and no AP broadcast.

**Independent test**: See [quickstart.md Scenario 2](quickstart.md#scenario-2-automatic-connection-on-subsequent-boot).

- [X] T016 [US2] Implement STA-only boot path in `wifi_config_init()` — when `load_credentials()` returns true: call `cyw43_arch_enable_sta_mode()`, attempt connection with 20 000 ms timeout using stored credentials; on success: return `WIFI_CONFIG_CONNECTED` without starting AP; on failure: fall through to `wifi_config_start_ap()` with `error_reason=RECOVERY` — `wifi_config/wifi_config.c`

**Checkpoint**: Store valid credentials via Phase 3 flow; reboot; confirm `pov-leds-setup` does NOT appear and device reaches network.

---

## Phase 5: User Story 3 — Invalid Credentials Recovery (Priority: P2)

**Goal**: Stored credentials that fail to connect cause the device to fall back to AP mode with a clear error message.

**Independent test**: See [quickstart.md Scenario 3](quickstart.md#scenario-3-wrong-password-recovery).

- [X] T017 [US3] Update `wifi_http.c` SSI handler for `GET /` — check `error_reason` module variable: if `FAILED_AUTH` inject "Incorrect password. Please try again." banner; if `FAILED_TIMEOUT` inject "Network not found or out of range. Move closer to your router." banner; if `RECOVERY` (boot-time failure) inject "Previous connection failed. Please reconfigure." banner; if `SAVE_FAILED` inject "Connected but failed to save settings. Please try again." banner — `wifi_config/wifi_http.c`
- [X] T018 [P] [US3] Verify in `wifi_config.c` that `save_credentials()` is called ONLY inside the `PICO_OK` branch of the STA connection outcome and nowhere else; add a comment at the call site citing FR-009 — `wifi_config/wifi_config.c`

**Checkpoint**: Store wrong password; reboot; AP reappears; config page shows "Previous connection failed" banner.

---

## Phase 6: User Story 4 — Invalid Credential Submission Rejected (Priority: P2)

**Goal**: A form submission with an empty or invalid SSID returns a 400 error, makes no connection attempt, and leaves the AP running for retry.

**Independent test**: See [quickstart.md Scenario 5](quickstart.md#scenario-5-empty-ssid-rejected).

- [X] T019 [US4] Verify POST `/connect` server-side validation in `wifi_http.c` — effective SSID empty or > 32 chars → respond with `400` HTML error page, do NOT set `pending_connect`, do NOT call `wifi_config_connect_sta()`; password > 63 chars → same 400 response — `wifi_config/wifi_http.c`
- [X] T020 [P] [US4] Confirm AP remains running after a 400 validation rejection — verify `wifi_config_start_ap()` loop continues and serves fresh `GET /` after a rejected POST — `wifi_config/wifi_config.c`

**Checkpoint**: Submit empty SSID; receive 400 response; AP still visible; form reloads and is usable.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T021 [P] Add "Refresh networks" link to the config page HTML — a plain `<a href="/">Refresh networks (N found)</a>` that reloads the page and re-triggers scan — `wifi_config/wifi_web.h`
- [X] T022 [P] Validate `GET /scan` optional JSON endpoint in `wifi_http.c` — if `LWIP_HTTPD_CGI` supports GET handlers, register `GET /scan` returning `application/json` array from current scan results (ssid, rssi, secured); otherwise leave as a documented future enhancement — `wifi_config/wifi_http.c`
- [X] T023 Verify single-command build — run `ninja -C build` from clean state; fix any missing `target_include_directories`, header search paths, or unresolved symbols — `CMakeLists.txt`
- [X] T024 [P] Verify flash write/erase runs from RAM — confirm `save_credentials()` and `clear_credentials()` are decorated with `__no_inline_not_in_flash_func` and that interrupts are disabled via `uint32_t ints = save_and_disable_interrupts()` / `restore_interrupts(ints)` around erase+write — `wifi_config/wifi_flash.c`
- [X] T025 Validate all quickstart scenarios pass on hardware — work through [quickstart.md](quickstart.md) Scenarios 1–6 on physical Pico W hardware; record AP startup time from power-on on a fresh-erased device (must be ≤10 s per SC-002); document any deviations

---

## Dependencies (Story Completion Order)

```
Phase 1 (Setup)
    └── Phase 2 (Foundational: flash layer)
            ├── Phase 3 (US1: AP + scan + web — MVP)
            │       └── Phase 4 (US2: STA boot path — extends US1's connect logic)
            │               ├── Phase 5 (US3: error recovery — extends US1/US4 AP loop)
            │               └── Phase 6 (US4: submission validation — extends US1 HTTP handler)
            │                       └── Phase 7 (Polish)
            └── Phase 4 also depends directly on Phase 2 (flash read)
```

---

## Parallel Execution Examples

**Phase 3 parallel group** (T008, T009, T010 can all start simultaneously):
- Developer A: `wifi_scan.h/c` (T008, T009)
- Developer B: `wifi_web.h` (T010)
- Developer C: `wifi_dns.h/c` (T012)

After T008–T010 complete → Developer A continues with `wifi_http.h/c` (T011).

---

## Implementation Strategy

**MVP scope** = Phase 1 + Phase 2 + Phase 3 (T001–T015)

Phase 3 alone delivers a complete, independently demonstrable provisioning flow: device boots into AP, user selects network from scan, device validates and saves credentials. All four user stories extend or validate this core flow.

**Recommended order within Phase 3**:
1. T010 (wifi_web.h) first — fastest to write, unblocks everything else.
2. T008–T009 (wifi_scan) in parallel.
3. T011 (wifi_http) after scan and web are done.
4. T012 (wifi_dns) in parallel with T011.
5. T013–T015 (wifi_config.c) last — orchestrates everything.
