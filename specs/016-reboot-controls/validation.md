# Validation Record: Reboot Controls

## Baseline (T001)

- `ninja -C build`: passed before feature-016 source changes.
- Firmware size: text 401128, data 20, BSS 68512, total 469660 bytes.
- Fixed portal page buffer: `s_page_buf` remains 16384 bytes.

## Automated validation

- `g++ -std=c++17 -Iwifi_config tests/wifi_reboot_controls_test.cpp wifi_config/wifi_sta_web.c`: passed.
  - Covers enabled/disabled Reboot presentation, confirmation, accepted, conflict,
    undersized-buffer handling, Blink Active/Idle preservation, and JSON frequency removal.
- `g++ -std=c++17 -I. tests/pov_adaptive_rendering_test.cpp pov_clock.cpp pov_clock_renderer.cpp`: passed.
- `ninja -C build`: passed after feature-016 source changes.
- `git diff --check`: passed.

## Final static-memory check (T016)

- Firmware size: text 402960, data 236, BSS 68500, total 471696 bytes.
- BSS decreased by 12 bytes from the baseline; no new page/request buffer or heap allocation was introduced.
- Production-code scan found no `blink_frequency_hz`, `blink_hz`, or `frequency_hz` references. The remaining phrases occur only in assertions proving their absence.

## Hardware validation still required

- Confirm/cancel, acknowledgement-before-disconnect, normal reboot mode, less-than-five-second reset start, browser-disconnect completion, saved Wi-Fi/brightness persistence, duplicate POST handling, OTA receiving/validating/ready conflicts, and USB/OTA regression require a connected Pico W and browser session.
