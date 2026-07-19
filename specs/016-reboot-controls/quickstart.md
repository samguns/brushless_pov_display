# Quickstart: Reboot Controls Validation

## Prerequisites

- Active feature: `specs/016-reboot-controls`
- Configured Pico SDK 2.2.0 build directory
- Pico W connected to a test Wi-Fi network
- Browser access to the device's STA management address
- Saved Wi-Fi credentials and a known non-default brightness value for
  persistence checks

## 1. Host validation

Run the focused reboot-control/page-builder host test created by the
implementation tasks. Verify:

- Overview retains Blink Active/Idle, Current Clock, Rotation Speed, network,
  and address fields.
- Overview and status JSON contain neither `Blink Frequency` nor
  `frequency_hz` nor an Hz placeholder.
- Settings renders an enabled Reboot action when safe and a disabled action with
  a reason for receiving, validating, and ready OTA states.
- Confirmation, accepted, and conflict pages contain the contract text and fit
  the existing fixed buffer; undersized buffers fail cleanly.
- Pending-action tests accept one normal target, reject duplicates and USB/OTA
  conflicts, and never turn Cancel into a mutation.

## 2. Build and static checks

```powershell
ninja -C build
arm-none-eabi-size build/pov_leds.elf
arm-none-eabi-nm -S --size-sort build/pov_leds.elf
git diff --check
```

Confirm there is no new page/request buffer, heap use, or build dependency.
Record the final persistent static-RAM change; it must not increase overall.

## 3. Overview and status regression

1. Open Overview with calibrated time and measured rotation.
2. Verify Blink Active/Idle remains visible.
3. Verify Blink Frequency and all frequency values/placeholders are absent.
4. Fetch `/status` and verify `blink.active` remains while `frequency_hz` is
   absent.
5. Verify Current Clock, Rotation Speed, SSID, address, theme, and layout remain
   correct.

## 4. Cancel flow

1. Open Settings and select Reboot.
2. Verify the confirmation distinguishes normal reboot from USB/OTA update and
   warns about temporary display/network interruption.
3. Select Cancel.
4. Verify Settings returns, uptime continues, and no restart occurs.

## 5. Accepted reboot and persistence

1. Record saved SSID and brightness.
2. Confirm Reboot and timestamp acknowledgement receipt and reset start.
3. Verify the acknowledgement is visible before disconnection.
4. Verify reset starts within five seconds even if the browser closes after the
   acknowledgement.
5. Reconnect after startup and verify the normal application is running, not
   USB BOOTSEL or an OTA-install flow.
6. Verify SSID and brightness equal their pre-reboot values.

## 6. Duplicate and conflict handling

1. Double-submit or replay `POST /reboot`; verify only one restart occurs.
2. Start an OTA upload and verify Reboot is unavailable during receiving and
   validating.
3. Verify direct `GET /reboot` or `POST /reboot` returns conflict while OTA is
   receiving, validating, or ready to restart.
4. Force a failed OTA trial and verify manual reboot becomes available without
   aborting or installing the failed package.
5. Re-run USB BOOTSEL update and Wi-Fi OTA happy paths to confirm both existing
   update modes retain their behavior.

Slow Wi-Fi reconnection after reset is recorded separately; it does not by
itself mean the accepted reboot failed.
