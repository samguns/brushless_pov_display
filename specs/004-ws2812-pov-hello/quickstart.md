# Quickstart & Validation: WS2812 POV Hello Demo

**Date**: 2026-06-26
**Feature**: 004-ws2812-pov-hello

## Prerequisites

- Raspberry Pi Pico W with WS2812 strip wiring.
- Build environment configured for Pico SDK 2.2.0.
- Development validation build with USB stdio enabled.
- USB serial terminal for runtime logs.

## Build

```bash
ninja -C build
```

Expected: successful build and updated firmware image.

## Flash

```bash
picotool load build/pov_leds.uf2 -fx
```

## Validation Scenario 1: Output-path readiness

1. Boot firmware with WS2812 strip connected.
2. Observe startup logs and LED updates.

Expected:
- Output-path readiness is reported.
- LED updates are stable with no hangs.

## Validation Scenario 2: Hello playback timing

1. Observe character playback sequence on the POV output.
2. Measure per-character hold duration for one full cycle.

Expected:
- Sequence order is H, e, l, l, o.
- Each character duration is 1.0 s +/-0.1 s.

## Validation Scenario 3: Continuous looping

1. Let demo run for at least 3 full cycles.

Expected:
- Transition from o back to H occurs cleanly.
- No missing character transitions.

## Validation Scenario 4: Bounds behavior

1. Test configured LED counts 0, 1, 57, and >57.

Expected:
- Valid counts render correctly.
- Out-of-range requests are bounded safely.
- System remains responsive with recoverable error logs.

## Validation Scenario 5: Stability soak

1. Run demo continuously for 5 minutes with active count set to 57.

Expected:
- No crashes or watchdog resets.
- Timing remains within tolerance envelope.

## Validation Scenario 6: Transport and timing-source evidence

1. Review runtime logs and implementation traces for frame transport path.
2. Confirm timing configuration is computed from runtime clock source.

Expected:
- Frame transport uses DMA -> TX FIFO -> PIO without CPU bit-banging fallback.
- Timing source references `clock_get_hz(clk_sys)` and does not use hardcoded system-clock literals.

## References

- Spec: [spec.md](spec.md)
- Plan: [plan.md](plan.md)
- Research: [research.md](research.md)
- Data model: [data-model.md](data-model.md)
- Behavior contract: [contracts/pov-demo-contract.md](contracts/pov-demo-contract.md)
