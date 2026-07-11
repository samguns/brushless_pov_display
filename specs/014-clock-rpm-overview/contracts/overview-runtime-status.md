# Contract: Overview Runtime Status

## Application publication

The application publishes clock state non-blockingly on each main-loop pass:

- `available`: whether device time is calibrated.
- `text`: bounded `HH:MM:SS` text when available.

Existing rotation publication remains:

- `available`: whether a valid Hall measurement has ever existed.
- `rpm`: nearest-whole measured RPM, or zero after established rotation stops.

Publication allocates no memory and does not change either source subsystem.

## Runtime propagation

The Wi-Fi runtime copies both statuses into its fixed state. While the STA portal
is active, its ordinary runtime update copies them into HTTP-owned fixed state.
The HTTP request handler passes that snapshot to the Overview builder.

## Overview presentation

`GET /` retains all existing content and renders:

| Label | Condition | Value |
|---|---|---|
| `Current Clock` | clock available | `<HH:MM:SS> CST` |
| `Current Clock` | clock unavailable | `--:--:-- CST` |
| `Rotation Speed` | RPM available | `<rpm> RPM` |
| `Rotation Speed` | RPM unavailable | `-- RPM` |

The page remains self-contained, request-driven, responsive, and bounded by its
existing fixed response buffer.

