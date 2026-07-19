# Validation: Web Log Viewer

**Feature**: 020-web-log-viewer

## Baseline and implementation shape

- Pre-feature development ELF: `text=405088`, `data=156`, `bss=71912` bytes.
- The implementation adds a hardware-independent `pov_log.h/.c` store and a
  pure `wifi_log_web.h/.c` page/JSON builder. The existing `wifi_sta_web.c`
  owns shared portal navigation; `wifi_sta_http.c` owns parsing and transport.
- The existing STA `s_page_buf[16384]` is reused for Logs HTML and JSON. No
  second response buffer, heap allocation, SSE connection, or WebSocket was
  added.

## Automated results

Feature-focused tests and both firmware builds passed on 2026-07-19. The
adaptive Hall regression also passed before an unrelated concurrent workspace edit;
the final rerun caveat is recorded immediately below.

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic -I. tests/pov_log_test.cpp pov_log.c -o build/pov_log_test.exe
./build/pov_log_test.exe
# pov log tests passed

g++ -std=c++17 -Wall -Wextra -pedantic -I. tests/wifi_log_viewer_test.cpp pov_log.c wifi_config/wifi_log_web.c -o build/wifi_log_viewer_test.exe
./build/wifi_log_viewer_test.exe
# wifi log viewer tests passed (page bytes=6236, max batch bytes=1466)

g++ -std=c++17 -Wall -Wextra -pedantic -I. -Iwifi_config tests/wifi_reboot_controls_test.cpp wifi_config/wifi_sta_web.c -o build/wifi_reboot_controls_test.exe
./build/wifi_reboot_controls_test.exe
# wifi reboot controls tests passed

g++ -std=c++17 -Wall -Wextra -pedantic -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test.exe
./build/pov_render_test.exe
# initially passed; final rerun fails after pov_clock.h was externally changed
# from 480/600/800 RPM to 48/60/80 RPM (preserved, not modified here)

cmake --build build -j 4
cmake -S . -B build-release-ninja -G Ninja -DPOV_DEMO_DEV_USB_STDIO=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release-ninja -j 4
git diff --check
```

The ring tests cover empty/partial/full state, 1,000 appends, newest-128
overwrite, ordering, timestamps, source labels, boot reset, invalid UTF-8,
exact 100-byte boundary handling, UTF-8-safe truncation, size bounds, and password/token/authorization redaction.
The web tests cover empty history, 16-entry pagination, zero duplicates, exact
gaps, metadata-only pause probes, future cursors, boot mismatch (including an
empty new boot), JSON/HTML-significant escaping, bounded output failure,
`textContent`, direct bounded JSON byte escaping, one sequential fetch, one-second polling, four-second abort,
1/2/4/5-second retry, strict session/sequence validation, fixed paused cursor/scroll,
immediate Resume catch-up, visible session/uptime/empty/truncation indicators,
client-only Clear behavior, saved light/dark theme bootstrap, restart
clearing/marker, responsive CSS, and the 128-row DOM cap.

## Static-memory results

`arm-none-eabi-size` after the final builds:

| Image | text | data | bss |
|---|---:|---:|---:|
| Development (`build/pov_leds.elf`) | 416,360 | 148 | 87,296 |
| USB-disabled (`build-release-ninja/pov_leds.elf`) | 401,476 | 184 | 84,728 |

- Development BSS delta from the recorded baseline: `87296 - 71912 = 15384`
  bytes, below the 16 KiB feature cap.
- Sorted-symbol evidence reports `s_log` size `0x3c18 = 15,384` bytes, below
  the 15,392-byte logger-state cap.
- The STA response buffer remains one `s_page_buf` symbol of `0x4000 = 16,384`
  bytes. The separate existing AP provisioning buffer remains `0x1c00` bytes.
- The USB-disabled Ninja build contains `POV_LOG_CONSOLE=0`; the USB wait and
  safe console mirror are compiled out.

## Security and timing-context audit

- The former complete captive-portal POST-body diagnostic was removed. Only
  the received byte count is logged; Wi-Fi password and admin-token values are
  never passed to the logger.
- In-scope producer calls log SSIDs and bounded lengths/status values, but no
  credential, authorization header, full body, firmware payload, or update
  chunk data. The logger also redacts recognized password, passphrase,
  admin-token, authorization, and bearer patterns as defense in depth.
- JSON escapes quote, backslash, controls, `<`, `>`, and `&`; valid non-ASCII
  UTF-8 remains valid. The page constructs every device row with `textContent`
  or `createTextNode` and contains no `innerHTML` sink.
- `GET /logs` and `GET /logs/updates` are read-only. Responses include exact
  `Content-Length`, `Cache-Control: no-store`, and
  `X-Content-Type-Options: nosniff`. Successful update polls are not logged.
- No logging was added to the Hall ISR, PIO/DMA callbacks, per-column rendering,
  successful polling loop, or OTA payload-chunk path. Firmware-update logs are
  limited to begin/failure/validation/install/boot-result transitions.

## Hardware-only gates (pending)

These require a powered rotating PCB, shared test Wi-Fi, browser, and logic
analyzer and were not claimed from host validation:

- Open `/logs` on the spinning assembly with USB physically disconnected and
  confirm retained startup plus live events.
- Confirm at least 95% of entries appear within two seconds and all within five
  seconds on the target network.
- Run pause/resume, Wi-Fi loss/recovery, reboot/new-session, narrow viewport,
  and 15-minute viewer-soak trials in a real browser.
- Compare Hall-to-column timing for at least 100 revolutions at 480, 600, and
  800 RPM; require jitter below one microsecond and zero missing Hall events or
  rendered columns attributable to logging/viewing.

## Final convergence result

- All 34 implementation and convergence tasks are complete.
- The final Spec Kit comparison found no remaining implementation gaps across
  the specification requirements, success criteria, acceptance scenarios,
  implementation plan, HTTP contract, and project constitution.
- The focused host suites and both firmware build variants pass. Physical
  spinning-board, browser/network, and logic-analyzer gates remain pending as
  listed above.
- The current adaptive-rendering regression is outside this feature: a
  concurrent workspace edit changed the thresholds in `pov_clock.h` from
  480/600/800 RPM to 48/60/80 RPM. That user-owned change was preserved.

No Git commit, push, issue, or pull request was created.
