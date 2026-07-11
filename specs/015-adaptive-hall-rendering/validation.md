# Validation Log: Adaptive Hall-Synchronized Rendering

## Automated verification

- Focused host timing test: PASS. Covers inclusive 480/600/764/800 RPM,
  479/801 rejection, Hall phase/generation propagation, generation-aware
  stability, stale recovery, rational phase mapping, 100-revolution return,
  delayed-column skipping, re-anchoring, invalid inputs, and transport budget.
- Firmware build: PASS with ninja -C build.
- Existing clock/RPM Overview host test: PASS; response remains 8,834 bytes.
- Source whitespace validation: PASS with git diff --check.

## Fixed-memory impact

- Host structure sizes: hall_rotation_measurement_t = 40 bytes,
  pov_clock_rotation_t = 32 bytes, pov_clock_renderer_t = 240 bytes.
- Relative to the prior documented/derived layouts, measurement adds 16 bytes,
  rotation adds 8 bytes, and the 40-column renderer saves about 32 bytes versus
  48 columns. The driver adds one fixed 64-bit transfer-ready timestamp.
- Linked firmware totals: text 401,024 bytes, data 124 bytes, BSS 68,508 bytes.
- No heap, queued column history, or second polar frame was added.

## Hardware validation

- 480/600/approximately 764/800 RPM readability and 100-revolution drift: pending.
- Speed-transition settling and injected-delay recovery: pending.
- Hall-to-column jitter measurement against the constitution's sub-microsecond
  budget: pending; software tests prove no cumulative drift, not physical jitter.
- 15-minute stability soak: pending.

## Residual risks

- The 800 RPM budget leaves about 115 microseconds after 57-pixel wire and latch
  time, so target logic-analyzer validation is mandatory.
- Blocking Wi-Fi scan/reconfiguration may cause visible skipped columns while it
  runs. Phase mapping recovers without persistent drift afterward, but cannot
  display columns retroactively during a blocking operation.
