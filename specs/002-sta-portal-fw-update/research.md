# Research: STA Management Portal & Firmware Update

**Date**: 2026-06-25
**Feature**: 002-sta-portal-fw-update

---

## RES-001: Protecting WiFi Credentials Across UF2 Firmware Updates

**Decision**: Reserve the last 4 KB flash sector at link time by providing a custom linker
script (`memmap_wifi_creds.ld`, a minimal derivative of the SDK's `memmap_default.ld`)
whose `FLASH` region length is `PICO_FLASH_SIZE_BYTES - 4096`, plus an explicit
`ASSERT` that fails the build if the firmware image would overflow the boundary. No
runtime partition manager, NVS layer, or wear-levelling is added.

**Rationale**:
- The RP2040 UF2 bootloader (and `picotool load`) only writes the flash pages that are
  actually present in the UF2 file. The credential sector is never emitted into the UF2
  because no linker section is placed there, so a normal firmware update physically
  cannot erase or overwrite it. (Confirmed by SC-006: the region is unchanged before/after
  an update.)
- Feature 001 already stores the credential record at `PICO_FLASH_SIZE_BYTES - 4096`. By
  shortening only the *linkable* `FLASH` length (keeping `ORIGIN = 0x10000000` and the
  same `PICO_FLASH_SIZE_BYTES` value used by `wifi_flash.c`), the offset stays identical
  and forward compatibility (FR-004) is preserved — old and new firmware read the same
  bytes.
- A linker `ASSERT` gives the build-time guarantee required by FR-003/SC-005: a firmware
  that grows past `ORIGIN + (PICO_FLASH_SIZE_BYTES - 4096)` fails to link with a clear
  message instead of silently colliding with credentials at runtime.

**Linker mechanism**:
- The SDK `memmap_default.ld` pulls its `FLASH` line from a generated
  `pico_flash_region.ld` (`FLASH(rx) : ORIGIN = 0x10000000, LENGTH = (2 * 1024 * 1024)`).
- The custom script duplicates `memmap_default.ld` but replaces the `INCLUDE
  "pico_flash_region.ld"` line with an explicit reserved-length `FLASH` definition and
  adds, after the existing `.flash_end`/`__flash_binary_end` section, an
  `ASSERT(__flash_binary_end <= ORIGIN(FLASH) + LENGTH(FLASH), "ERROR: firmware overflows
  reserved WiFi credential sector")`.
- Wired via `pico_set_linker_script(pov_leds ${CMAKE_CURRENT_LIST_DIR}/memmap_wifi_creds.ld)`.

**Alternatives considered**:
- Lowering `PICO_FLASH_SIZE_BYTES`: rejected — it would shift the credential offset used by
  `wifi_flash.c`, breaking forward compatibility with records already written by feature 001.
- CMake POST_BUILD size check on the `.bin`: acceptable per FR-003's "or equivalent
  build-time check" wording, but rejected as primary because it fires after a successful
  link and gives a less precise message than a linker `ASSERT`. (May be added as a
  secondary guard, but not required.)
- Full partition table / NVS / littlefs: rejected by the spec's own clarification (passive
  linker boundary only).

---

## RES-002: STA-Mode HTTP Server Reuse vs. New Module

**Decision**: Implement a dedicated STA-mode server in new files `wifi_sta_http.c` /
`wifi_sta_web.c`, reusing the proven raw-lwIP TCP pattern from the AP-mode `wifi_http.c`
(accept → accumulate request → dispatch by method/path → `send_response` → `tcp_close`).

**Rationale**:
- The AP-mode server (`wifi_http.c`) and STA-mode server run in mutually exclusive phases:
  AP server only during provisioning, STA server only after a successful connection. They
  never coexist, so there is no port-80 conflict (matches the spec assumption).
- The page sets differ entirely (provisioning form + scan list vs. status + update
  confirmation). Forcing both into one file would entangle unrelated state machines and
  violate Constitution Principle III.
- The existing raw-TCP send/parse helpers are small and well understood; duplicating the
  ~80-line request-handling skeleton is cheaper and clearer than generalising it.

**Pattern reused from `wifi_http.c`**:
- Single-client `s_client_pcb`, request accumulation in a static buffer, `on_accept` /
  `on_recv` / `on_client_err` callbacks.
- `send_response()` with `Connection: close`, served from static page buffers.
- Driven by `cyw43_arch_poll()` + a module `*_poll()` call in the main loop.

**Alternatives considered**:
- lwIP `httpd` with CGI/SSI: rejected for the same reasons as feature 001 RES-001 — the raw
  TCP approach is already in place and avoids `makefsdata` tooling for two tiny pages.
- Generalising `wifi_http.c` to serve both modes: rejected — couples two independent
  lifecycles.

---

## RES-003: Triggering USB Mass-Storage (Firmware Update) Mode

**Decision**: On confirmed update (`POST /update`), send the HTTP response, allow lwIP to
flush, then call `reset_usb_boot(0, 0)` from `pico/bootrom.h` to reboot into the RP2040 ROM
USB MSD (BOOTSEL) bootloader.

**Rationale**:
- `reset_usb_boot(0, 0)` is the SDK-blessed way to enter the ROM bootloader programmatically;
  it presents the `RPI-RP2` mass-storage drive that accepts a UF2 drop — exactly equivalent
  to holding BOOTSEL on power-up (FR-010, matches the spec assumption).
- First argument `0` = no GPIO activity mask; second `0` = enable both USB MSD and PICOBOOT
  interfaces.
- The response must be fully written and the TCP connection closed *before* the reset, or
  the browser is left without a reply. The same "flush then act" delay used by the AP-mode
  `POST /connect` path (poll lwIP for ~600 ms) is reused.

**60-second timeout (FR-013)**: The RP2040 ROM bootloader does not auto-return to the
application after a timeout — once in MSD mode it stays there until a UF2 is written or the
device is power-cycled. To honour FR-013 ("reboot back to STA after 60 s with no UF2"), the
countdown and the auto-return are implemented **before** entering MSD mode: the confirmation
page shows a 60 s JS countdown; the device only calls `reset_usb_boot()` when the user
presses *Confirm*. If the user does nothing, a meta-refresh / JS redirect returns the page to
`/` after 60 s and no reboot occurs (the device simply keeps running STA). This satisfies the
user-facing intent of FR-013 (no accidental stranding) without depending on ROM-bootloader
behaviour the SDK cannot control. Documented as a design note in the contract.

**Alternatives considered**:
- `watchdog_reboot()` into a custom UF2 mode: rejected — there is no application-side UF2
  receiver; the ROM MSD path is the requirement.
- Counting down inside MSD mode: rejected — not controllable from application code once the
  ROM bootloader is active.

---

## RES-004: Printing the STA IP Address and SSID to Serial

**Decision**: After `wifi_config_init()` returns (STA connected), read the acquired IPv4
address from the STA netif and the connected SSID, and print both to USB serial within the
STA-run entry point.

**Rationale**:
- `cyw43_arch_lwip_poll` mode keeps the STA netif at `cyw43_state.netif[CYW43_ITF_STA]`
  (also `netif_default`). The DHCP-assigned address is available via
  `netif_ip4_addr(netif)` (or `cyw43_state.netif[CYW43_ITF_STA].ip_addr`).
- USB stdio is already enabled (`pico_enable_stdio_usb 1`), so `printf` reaches the host
  serial console — satisfying FR-005 / SC-002 (printed within 500 ms of the lease).
- The connected SSID is the one the device just connected with. `wifi_config.c` already has
  the `wifi_credentials_t` it used; it is passed/stored so the STA-run path and the status
  page can display it (FR-007).

**Implementation note**: `wifi_config_init()` currently `return`s the moment STA connects
(stored-credential path) or reboots (post-provisioning path). To make the connected SSID
available to the STA portal in both "already had credentials" and "just provisioned then
rebooted" cases, the device always reaches `wifi_config_run_sta()` after a normal STA boot,
which re-reads the stored credentials via `load_credentials()` for the SSID and reads the
live IP from the netif.

---

## RES-005: WiFi Drop Handling While Serving the STA Portal

**Decision**: While polling the STA portal, monitor `cyw43_tcpip_link_status(&cyw43_state,
CYW43_ITF_STA)`. On link loss, attempt a silent reconnect using the stored credentials; if
the link is not restored within 15 s, fall back to AP provisioning mode by invoking the
existing feature-001 recovery path (`wifi_config_init()` AP loop with `WIFI_ERR_RECOVERY`).

**Rationale**:
- Reuses the existing, tested recovery path rather than inventing a second one (FR-014,
  matches the spec assumption).
- `cyw43_arch_wifi_connect_timeout_ms()` with the stored credentials performs the silent
  reconnect; the existing `try_sta()` helper already encapsulates this.
- A 15 s budget matches the spec; the link-status poll is cheap and runs in the same loop as
  `wifi_sta_http_poll()`.

**Alternatives considered**:
- Immediate AP fallback on first drop: rejected — transient drops are common; silent
  reconnect first is the specified behaviour.
- Hard reboot on drop: rejected — needlessly disruptive and loses the chance for a quick
  reconnect.

---

## RES-006: Static RAM Budget Impact

**Decision**: The STA portal reuses the same buffer-sizing strategy as `wifi_http.c`:
a status/confirmation page buffer (~3 KB is ample for two small pages), a header buffer
(256 B), and a request accumulation buffer (1 KB). Net new static RAM is well under 8 KB.

**Rationale**:
- The two STA pages are far smaller than the AP config page (no 20-network list), so the
  page buffer can be smaller than `wifi_http.c`'s 7 KB. A 4 KB buffer is conservative.
- No new lwIP pools are introduced; the STA server uses the same lwIP instance already sized
  by feature 001's `lwipopts.h`.
- Combined with feature 001's ~22–26 KB, total WiFi-subsystem SRAM remains < 35 KB, leaving
  > 225 KB for future POV frame buffers (Constitution Principle IV).
