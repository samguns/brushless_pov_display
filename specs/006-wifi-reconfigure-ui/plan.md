# Implementation Plan: Wi-Fi Reconfiguration UI

**Branch**: `006-wifi-reconfigure-ui` | **Date**: 2026-06-29 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/006-wifi-reconfigure-ui/spec.md`

## Summary

Add a reconfiguration UI to the existing STA-mode management portal so the owner
can change the Wi-Fi network (SSID + password) at any time — without forcing a
boot-time credential failure and AP fallback. The UI offers manual SSID/password
entry plus an explicit "Scan" button that lists nearby networks (reusing the
existing `wifi_scan` module) for select-to-fill. Submitting validated credentials
test-connects to the new network; on success the credentials are persisted to the
reserved flash sector and the portal resumes on the new network, on failure the
device reverts to the previously working credentials and stays reachable. The
change endpoint is open (no admin token), consistent with the firmware-update
endpoint. The work reuses the existing `wifi_sta_web` / `wifi_sta_http` /
`wifi_config` / `wifi_flash` / `wifi_scan` modules and the single-command build.

## Technical Context

**Language/Version**: C11 / C++17 (existing project standard)

**Primary Dependencies**: Pico SDK 2.2.0, `pico_cyw43_arch_lwip_poll`, lwIP raw
TCP (`lwip/tcp.h`), `hardware_flash` (via `wifi_flash`), and the existing
`wifi_config` / `wifi_sta_web` / `wifi_sta_http` / `wifi_scan` modules

**Storage**: Existing reserved last-4 KB flash credential sector (`wifi_flash`,
`save_credentials`/`load_credentials`); V2 record includes ssid/password/admin_token

**Testing**: Manual hardware validation with a browser and two Wi-Fi networks;
build verification via `ninja -C build`; page/validation logic exercisable by
inspection of server-rendered output

**Target Platform**: Pimoroni Pico Plus 2 W (RP2350B + RM2) running the existing
firmware super-loop (`wifi_config_runtime_step` each iteration)

**Project Type**: Single embedded firmware target

**Performance Goals**: Owner completes a network change in under 60 s for a
reachable network; scan returns within a bounded window (a few seconds); input
validation is immediate; failed attempts return the device to reachable state

**Constraints**: Single radio — cannot serve old and new network (or scan) fully
concurrently, so apply/scan briefly perturb the live connection within bounded
windows; no heap in the flash-write path; persisted record is always complete
old-or-new (atomic erase+write+verify); change endpoint is open (no auth) per
clarification; SSID ≤ 32 chars, WPA2 password 8–63 chars; one HTTP client at a time

**Scale/Scope**: One management portal client at a time; up to 20 scan results
(existing `WIFI_SCAN_MAX_RESULTS`); WPA2/PSK networks only

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. PIO-First LED Drive | PASS (N/A) | Connectivity/UI feature; the LED output path is unchanged. |
| II. Timing Precision | PASS (N/A) | No column/LED timing affected; connect/scan use coarse bounded timeouts, no PIO dividers. |
| III. Hardware Abstraction | PASS | UI rendering (`wifi_sta_web`), HTTP routing (`wifi_sta_http`), connection control (`wifi_config`), storage (`wifi_flash`), and scanning (`wifi_scan`) remain separate responsibilities. |
| IV. Minimal & Deterministic Memory | PASS | Reuses existing fixed static page/request buffers and the bounded (≤20) scan-result array; no heap in the credential-write path. |
| V. Single-Command Build & Flash | PASS | Builds with the existing `ninja -C build`; no new build steps or sources required. |

**Post-design re-check**: PASS. Design reuses existing modules and buffers; the
only new behavior is bounded, owner-initiated blocking for apply/scan, documented
as an accepted tradeoff (see Complexity Tracking).

## Project Structure

### Documentation (this feature)

```text
specs/006-wifi-reconfigure-ui/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── http-api.md
├── checklists/
│   └── requirements.md
└── tasks.md            # created by /speckit-tasks (not this command)
```

### Source Code (repository root)

```text
wifi_config/
├── wifi_sta_web.h / .c     # add reconfiguration page + scan-list rendering + result pages
├── wifi_sta_http.h / .c    # add GET /wifi, GET /wifi?scan=1 (scan), wire POST /config to apply
├── wifi_config.h / .c      # add credential-apply (test-connect + persist/revert) entry point
├── wifi_scan.h / .c        # reused as-is for STA-mode scanning
└── wifi_flash.h / .c       # reused as-is for atomic credential persistence
```

**Structure Decision**: Extend the existing STA management portal rather than add
new modules. Page generation stays in `wifi_sta_web`, HTTP routing/parsing in
`wifi_sta_http`, and the connection/persist/revert logic in `wifi_config` (so the
HTTP layer never directly drives the radio or flash). Scanning reuses `wifi_scan`
unchanged.

## Complexity Tracking

| Item | Why Needed | Mitigation / Why Acceptable |
|------|------------|-----------------------------|
| Bounded blocking of the super-loop during apply/scan | Single radio cannot validate a new network (or scan) while keeping the old link fully live; test-connect must complete before persisting | Owner-initiated, infrequent action; operations are time-bounded and the WS2812 frame is latched/held, so a brief pause does not corrupt display state. No constitution principle is violated. |
| Open (unauthenticated) change endpoint | Explicit clarification decision for convenience, matching the firmware-update endpoint | Documented security tradeoff in spec (FR-009); no new mechanism introduced. |
