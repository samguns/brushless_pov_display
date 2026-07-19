# Quickstart & Validation: Web Log Viewer

**Feature**: 020-web-log-viewer

References: [plan.md](plan.md), [data-model.md](data-model.md), and
[contracts/log-viewer-http.md](contracts/log-viewer-http.md).

## Prerequisites

- Pico SDK 2.2.0 and ARM toolchain 14_2_Rel1 as configured by the repository.
- Raspberry Pi Pico W (or the already-supported Pimoroni alternate board), Hall
  input, WS2812 display, and a local Wi-Fi network shared with the test browser.
- Ninja, CMake, g++, picotool, and `arm-none-eabi-size`/`nm` available.
- For physical timing acceptance: a signal generator or controlled motor speed
  and a logic analyzer observing Hall input plus WS2812 burst/column starts.

## 1. Host validation

Compile and run the focused history and web-contract tests:

```powershell
g++ -std=c++17 -I. tests/pov_log_test.cpp pov_log.c -o build/pov_log_test.exe
./build/pov_log_test.exe

g++ -std=c++17 -I. tests/wifi_log_viewer_test.cpp pov_log.c wifi_config/wifi_log_web.c -o build/wifi_log_viewer_test.exe
./build/wifi_log_viewer_test.exe

g++ -std=c++17 -I. -Iwifi_config tests/wifi_reboot_controls_test.cpp wifi_config/wifi_sta_web.c -o build/wifi_reboot_controls_test.exe
./build/wifi_reboot_controls_test.exe

g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp -o build/pov_render_test.exe
./build/pov_render_test.exe
```

Required focused results:

- Empty, partial, full, wrap, and 1,000-append ring streams retain exactly the
  newest 128 entries in strict sequence order.
- Cursor pagination produces no duplicates/reversals; stale cursors return an
  exact gap; session mismatch resets continuity.
- Entry and full state compile-time/runtime sizes remain 120 and <=15,392 bytes.
- Exact-boundary and oversized messages are valid UTF-8, NUL-terminated, and
  visibly truncated; known password/token/authorization patterns are absent.
- Worst-case 16-entry JSON is complete and smaller than 16,384 bytes; quotes,
  controls, markup, and non-ASCII inputs parse correctly and remain inert.
- Logs navigation, status labels, pause/resume, client-only Clear, metadata
  polling, `textContent`, responsive styles, and undersized-buffer failure are
  present without removing Overview, Settings, reboot, or OTA behavior.

## 2. Firmware builds and static-memory evidence

Build the normal development image and a distinct USB-disabled release image:

```powershell
ninja -C build

cmake -S . -B build-release-ninja -G Ninja -DPOV_DEMO_DEV_USB_STDIO=OFF -DCMAKE_BUILD_TYPE=Release
ninja -C build-release-ninja

arm-none-eabi-size build/pov_leds.elf
arm-none-eabi-size build-release-ninja/pov_leds.elf
arm-none-eabi-nm -S --size-sort build-release-ninja/pov_leds.elf
git diff --check
```

Record the final `.bss` and `pov_log` symbols in feature validation. The current
pre-feature development baseline is 71,912 B BSS. The persistent RAM added for
history/capture state must be <=15,392 B and the total feature delta must remain
<=16 KiB. Confirm there is still one STA response buffer of 16 KiB and no new
heap call or response/history buffer.

## 3. Flash the no-USB acceptance image

```powershell
picotool load build-release-ninja/pov_leds.uf2 -fx
```

Disconnect USB, power the board from its normal rotating supply, and start the
display. Determine the station IP from the router/DHCP lease, then open:

```text
http://<device-ip>/logs
```

Expect a self-contained page that moves from Connecting to Live, shows the
current boot session and retained early startup messages, and continues without
USB or UART output.

## 4. Live history and latency

1. Trigger representative driver, clock, time-sync, Hall, health, Wi-Fi, DHCP,
   and update-state events. Do not perform a destructive update during the first
   pass.
2. Verify entries show sequence, boot-relative time, source, and message from
   oldest retained to newest.
3. Generate at least 100 timestamped test events and compare production time to
   browser receipt: >=95% must appear within 2 seconds and all within 5 seconds.
4. Run a 1,000-entry synthetic host stream and a >128-entry hardware burst.
   Verify no duplicates or reversals, newest 128 retention, and a visible exact
   overwritten-history gap.
5. Confirm successful `/logs/updates` polling does not create new log entries.

## 5. Pause, reconnect, and reboot

1. Scroll to an older row and select Pause.
2. Generate messages. Verify scroll and displayed cursor stay fixed while the
   unseen indicator rises; capture continues on the device.
3. Resume. Verify ordered catch-up, any required gap marker, and return to the
   newest row.
4. Disable the browser's Wi-Fi for >5 seconds. Verify Disconnected is visible
   and current rows remain. Restore Wi-Fi and verify same-session catch-up with
   no duplicates.
5. Disconnect again, reboot the board, and reconnect. Verify the old rows are
   cleared, a device-restarted marker appears, and the new session is not merged
   with the previous boot.

## 6. Security and inert rendering

In a controlled test, submit unique marker values through provisioning/config
inputs representing a password, admin token, authorization value, complete body,
and firmware payload marker. Also emit quotes, backslashes, markup, controls,
valid non-ASCII text, invalid UTF-8, and an overlength message.

Retrieve every retained page and inspect the browser DOM/network responses:

- zero secret marker values appear in the ring or JSON;
- the former complete AP POST-body diagnostic is absent;
- markup creates no element and executes no event/script;
- JSON remains valid and non-ASCII/truncation behavior is visible;
- responses are same-origin, `no-store`, and `nosniff`;
- GET log routes do not change settings, clear history, reboot, or begin update.

## 7. Physical timing acceptance

Test controlled Hall rates at 480, 600, and 800 RPM. For each rate, capture at
least 100 revolutions / 4,000 expected columns in both conditions:

1. web history capture active with no browser polling;
2. one browser continuously viewing Logs at the normal one-second cadence.

Compare Hall-to-column phase and adjacent column timing. Acceptance requires:

- measured maximum jitter remains below the project's existing 1 microsecond
  budget;
- zero missing Hall events and zero missing rendered columns attributable to
  logging/viewing;
- no new DMA-busy warning pattern or false Hall status transition;
- a 15-minute viewing soak remains responsive and within the bounded history.

Timing validation must use the USB-disabled image so console output cannot mask
or add delay.

## 8. Portal regression sweep

- Open Overview and Settings; Logs is one navigation action away and existing
  status, theme, responsive layout, brightness, network scan/change, and notices
  remain correct.
- Verify normal reboot, USB BOOTSEL update, and Wi-Fi OTA happy/conflict paths.
- Keep Logs polling during non-destructive regression actions and verify the sole
  HTTP slot remains recoverable; no request stays open as SSE/long polling.
- On a <=640 px viewport, messages wrap and controls/navigation remain usable
  without page-wide horizontal scrolling.
