# Quickstart: Validate Adaptive Hall-Synchronized Rendering

## Build and host checks

```powershell
ninja -C build
```

Run focused synthetic-timestamp checks covering:

1. 480, 600, 764, and 800 RPM are suitable; 479 and 801 are not.
2. Hall edge timestamp and sample generation propagate through rotation state.
3. Phase zero selects column 0; quarter/half/three-quarter revolution select
   the corresponding columns.
4. A delayed call skips to the current column without shifting later results.
5. New Hall edges re-anchor phase and speed-period changes settle immediately
   from the new sample.
6. Stability history updates once per Hall generation, not once per loop read.
7. A 57-LED transfer remains busy through wire time and latch; submission before
   transfer-ready time is rejected and submission at/after it is accepted.

## Hardware scenario 1: supported steady speeds

1. Calibrate time and run at 480, 600, approximately 764, then 800 RPM.
2. At each speed, observe at least 100 revolutions.
3. Confirm normal clock state, recognizable text, and no persistent angular drift.

## Hardware scenario 2: in-range speed changes

1. Change between supported speeds while rendering.
2. Confirm the clock settles within two revolutions without reboot.
3. Confirm no stale columns sweep through after a delayed iteration.

## Hardware scenario 3: boundaries and recovery

1. Run below 480 RPM, above 800 RPM, and stop rotation.
2. Confirm too-slow, too-fast, and unavailable fallbacks respectively.
3. Return to a supported speed and confirm normal output within two revolutions
   or one second, whichever occurs first.

## Hardware scenario 4: timing and soak

1. Measure column-strobe timing against the Hall reference on target hardware.
2. Record maximum jitter against the constitution's sub-microsecond budget.
3. Run for 15 minutes at a stable supported speed; confirm no hangs, resets,
   DMA overlap, or false unsupported-speed transitions.
