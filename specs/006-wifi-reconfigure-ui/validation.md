# Validation Log: Wi-Fi Reconfiguration UI

## Build Validation

- Completed: `ninja -C build` succeeds with no warnings/errors; the reconfiguration
  UI compiles and links into `pov_leds.elf` for the Pimoroni Pico Plus 2 W.
- STA page buffer enlarged to 8192 bytes so the reconfiguration page holds a full
  20-network scan list without truncation (T030).

## Scenario 1/2: Discoverability + Manual Change (US1)

- Pending hardware verification (Change Wi-Fi link present; manual SSID/password
  change connects and persists across reboot).

## Scenario 4: Safe Failure / Revert (US2)

- Pending hardware verification (bad password reverts to previous network; boots
  on previous network).

## Scenario 3: Scan + Select (US4)

- Pending hardware verification (Scan lists nearby networks with secured/open;
  select-to-fill; link recovers after scan).

## Scenario 5 + Confidentiality (US3)

- Pending hardware verification (empty/over-length rejected before reconnect;
  password never shown/logged in plaintext).

## Notes

- Apply is deferred: `POST /config` returns an "applying" page, the response is
  flushed, then the radio switches in the runtime loop. The outcome is surfaced as
  a banner on the status/reconfiguration pages once the device settles on the new
  or reverted network. (Addresses analysis finding S1.)
