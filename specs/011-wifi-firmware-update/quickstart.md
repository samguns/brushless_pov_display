# Quickstart & Validation: WiFi Firmware Update

## Prerequisites

- Pico SDK 2.2.0/toolchain, provisioned Pico W, admin token, same-LAN browser.
- USB access for the one-time migration and recovery path.

## Build and migration

```bash
ninja -C build
```

Expected artifacts include application/USB UF2, FOTA bootloader UF2, and a board-specific
`.povota`. Flash the bootloader and first OTA-capable application by USB in generated order.
This happens once; do not submit legacy `.uf2` files to the web form. Confirm build output
reserves the final 4 KB settings sector and both slots fit.

## Successful WiFi update

1. Open `/update`; check running identity and board.
2. Select a matching `.povota`, supply the admin token, and upload.
3. Confirm advancing progress, disabled duplicate submit, validation, and restart warning.
4. Restart and wait for WiFi reconnection.

Expected: the new identity appears; credentials and brightness persist; no USB interaction.

## Rejection and interruption

Submit empty, `.uf2`, truncated, oversized, and wrong-board packages; also submit twice.
Expected: a clear reason, no reboot, active firmware still reachable, and duplicate conflict.
Interrupt a valid upload: status eventually reports failure and no candidate is installed.

## Rollback and USB recovery

After a valid restart, reset before the candidate can commit; expect the next boot to restore
the prior image. Finally use BOOTSEL to flash generated USB artifacts after an invalid upload
or failed candidate boot, confirming the fallback is still usable.

See [data-model.md](data-model.md) and [HTTP contract](contracts/firmware-update-http.md).
