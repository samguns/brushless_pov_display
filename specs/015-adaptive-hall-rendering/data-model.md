# Data Model: Adaptive Hall-Synchronized Rendering

## Entity: Supported Rotation Envelope

| Field | Value | Rule |
|---|---:|---|
| Minimum speed | 480 RPM | Inclusive |
| Nominal reference | 600 RPM | Informational; not required for eligibility |
| Maximum speed | 800 RPM | Inclusive |
| Angular columns | 40 | Fixed compact layout for the supported transport budget |

## Entity: Rotation Timing Sample

| Field | Type | Rule |
|---|---|---|
| `period_us` | Positive integer | Latest bounded revolution period |
| `rpm` | Non-negative measurement | Used for inclusive envelope eligibility |
| `last_edge_us` | Monotonic timestamp | Latest accepted Hall reference event |
| `valid` | Boolean | True only after a complete fresh period exists |
| `stale` | Boolean | True after the existing stop timeout |

## Entity: Phase-Aware Rotation State

Hall timing also carries a monotonic sample generation copied from the accepted
edge count. Period-history and stability evaluation change only when this
generation advances, never merely because the main loop rereads the same sample.

| Field | Type | Rule |
|---|---|---|
| `period_us` | Positive integer | Copied only from fresh valid timing sample |
| `phase_reference_us` | Monotonic timestamp | Copied from `last_edge_us` |
| `status` | Rotation eligibility | Suitable only for fresh stable 480-800 RPM |
| `fresh`, `stable`, `within_range` | Booleans | Preserve existing state meanings |

## Entity: Angular Rendering Schedule

| Field | Type | Rule |
|---|---|---|
| `active_column` | 0-39 | Most recently selected angular column |
| `phase_reference_us` | Monotonic timestamp | Hall edge anchoring column zero |
| `rotation_period_us` | Positive integer | Period used for phase mapping |
| `phase_locked` | Boolean | False until a valid phase/period pair is accepted |

## Entity: LED Transport Readiness

| Field | Type | Rule |
|---|---|---|
| transfer_ready_us | Monotonic timestamp | Earliest safe next submission after wire time and latch |
| frame_duration_us | Positive integer | Derived from LED count, bits per pixel, bit rate, and latch interval |

- Transport is busy while DMA is active or current time precedes
  transfer_ready_us.
- Renderer phase selection may look ahead by frame_duration_us so the chosen
  column corresponds to expected LED presentation time.

### Phase mapping

```text
elapsed_us = now_us - phase_reference_us
phase_us = elapsed_us modulo rotation_period_us
target_column = floor(phase_us * 40 / rotation_period_us)
```

### State transitions

```text
Unavailable --fresh in-range sample--> Phase locked / normal
Phase locked --new Hall edge----------> Re-anchor column zero
Phase locked --period change----------> Recalculate from new phase/period
Phase locked --missed column(s)-------> Select current target; do not replay
Any --stale/out-of-range--------------> Fallback / phase unavailable
Fallback --fresh in-range sample------> Phase locked / normal
```

## Memory model

- Hall timestamp additions are fixed scalar fields.
- Renderer angular arrays contain 40 entries rather than 48.
- No dynamic allocation or queued column history is introduced.
- Final structure sizes and net fixed-memory change are recorded in validation.
