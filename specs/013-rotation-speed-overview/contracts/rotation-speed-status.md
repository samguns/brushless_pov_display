# Contract: Rotation Speed Overview Status

## Application publication

The application publishes the latest operator-facing rotation state as:

- `available`: whether at least one valid rotation measurement has existed.
- `rpm`: a non-negative, nearest-whole RPM value; zero when stopped after a
  measurement was established.

Publication is non-blocking, allocation-free, and safe to call every main-loop
iteration. It does not alter Hall capture or rotation derivation.

## Runtime propagation

When the STA portal is active, its normal runtime status update copies
`available` and `rpm` into HTTP server status alongside connectivity and blink
values. A request uses the most recently copied status.

## Overview presentation

`GET /` continues to render all existing Overview content and adds exactly one
metric card:

| Label | Condition | Value |
|---|---|---|
| `Rotation Speed` | `available == true` | `<rpm> RPM` |
| `Rotation Speed` | `available == false` | `-- RPM` |

The response remains a complete self-contained page using the existing layout,
theme, fixed page buffer, and no automatic browser polling.

