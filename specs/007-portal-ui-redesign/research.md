# Phase 0 Research: Management Portal UI/UX Redesign

**Feature**: 007-portal-ui-redesign

All spec clarifications were resolved in the 2026-06-29 session, so there are no
open `NEEDS CLARIFICATION` items. This document records the technical decisions
that shape Phase 1 design.

## RES-001 — Where to apply LED brightness

**Decision**: Add a global brightness scalar (0–255) to `ws2812_driver_t` with a
setter (`ws2812_driver_set_brightness`) and apply it by scaling each GRB byte of
every frame word inside `ws2812_driver_submit_frame` (e.g. `c = c * b / 255`).

**Rationale**: Keeps the change at the driver boundary (Principle III) while
touching only the data path, never the PIO program or clock divider — so WS2812
bit timing and rotation/column timing are unaffected (Principles I & II). The
main render loop in `pov_leds.cpp` keeps calling `pov_demo_render_frame` →
`submit_frame` unchanged; brightness is transparent to display logic. Scaling 57
words is negligible cost in the existing super-loop.

**Alternatives considered**:
- Scale inside `pov_demo_render_frame`: rejected — couples brightness to the demo
  content module and would need re-plumbing for any future frame source.
- Scale in `pov_leds.cpp` before submit: workable but spreads driver concerns
  into the application loop; the driver setter is cleaner and centralized.
- Hardware/global-current dimming: not available for plain WS2812 (no global
  brightness register like APA102), so data scaling is the only option.

## RES-002 — Persisting brightness across reboot

**Decision**: Extend the flash credential record to a **V3** layout that appends
a `uint8_t brightness` byte (plus version bump). `load_credentials` accepts V1,
V2, and V3 (defaulting brightness to a safe value for older records); add a
helper to persist brightness while preserving the current SSID/password/token.
Persist only when the submitted brightness **differs** from the stored value.

**Rationale**: Reuses the already-reserved 4 KB sector and the existing atomic
erase+write+verify path (Principle IV). One byte keeps the record (172 B) far
within the sector. Write-on-change avoids flash wear from repeated submits.

**Alternatives considered**:
- Separate flash sector for settings: rejected — wastes a sector and adds a
  second write path; the credential sector has ample room.
- Don't persist (session only): rejected — violates FR-017 (must survive reboot).
- Persist on every slider movement: rejected — flash-wear risk; the slider
  submits a single committed value per change instead.

## RES-003 — Theme (Dark/Light) switching

**Decision**: Implement entirely client-side. Define CSS custom properties for
both themes on a root selector; default to **dark**. A small inline `<script>`
toggles a `data-theme="light"` attribute on `<html>` and stores the choice in
`localStorage`, re-applied on load. No device endpoint or storage.

**Rationale**: Matches the clarification (theme is client-side; dark default).
Adds no device state and no flash writes; uses the same minimal-inline-JS
approach the portal already uses (e.g. the `/update` countdown). Fully offline.

**Alternatives considered**:
- Server-rendered theme with a persisted device preference: rejected —
  unnecessary device state and flash writes for a purely cosmetic browser choice.
- CSS `prefers-color-scheme` only: rejected — design requires an explicit toggle.

## RES-004 — Static page buffer sizing

**Decision**: Keep a single statically-allocated page buffer but raise
`STA_PAGE_BUF_SIZE` from 8 KB to **16 KB**, and keep the existing overflow guard
(builders return `-1` / the handler sends an error rather than truncating).
Factor all shared styling into one compact (whitespace-trimmed) CSS block reused
by every page so the per-page delta is mostly content, not repeated CSS.

**Rationale**: The Overview is small, but the Settings page (three cards + up to
20 scan rows, each repeating the SSID for select-to-fill) plus the richer CSS is
the worst case. 16 KB gives comfortable headroom; on a 520 KB-SRAM RP2350B a
fixed 16 KB array is immaterial and remains deterministic (Principle IV). Final
size will be confirmed by inspecting the largest page's `Content-Length` during
validation and adjusted if needed, but never made dynamic.

**Alternatives considered**:
- Chunked/streamed responses: rejected — larger change to the TCP write path for
  little benefit at this scale; one client, bounded pages.
- Keep 8 KB: rejected — high risk of truncating the full scan-list Settings page
  once the dark CSS is added.

## RES-005 — Information architecture / routing

**Decision**: Map the two design frames onto routes:
- `GET /` → **Overview** screen (re-skinned status page).
- `GET /settings` → **Settings** screen (Display + System + Network cards);
  `GET /settings?scan=1` triggers the scan; keep `GET /wifi` as an alias.
- `POST /config` (Wi-Fi change) unchanged; `GET /update` + `POST /update`
  (firmware) unchanged, reached from the System card; re-skin their pages.
- `POST /display` → **new** endpoint accepting the brightness value.
The sidebar nav links Overview→`/` and Settings→`/settings` on every screen.

**Rationale**: Reuses existing handlers and behavior (FR-013) while presenting
the consolidated Settings layout the design shows. The alias avoids breaking any
bookmarked `/wifi` link. A dedicated `POST /display` keeps brightness separate
from credential submission.

**Alternatives considered**:
- Single-page app with client routing: rejected — heavier JS, against the
  minimal-inline-JS, server-rendered model.
- Overload `POST /config` with brightness: rejected — mixes unrelated concerns
  and complicates validation.

## RES-006 — Typography & iconography without external assets

**Decision**: Use a system font stack — a sans-serif UI stack for labels/headings
and a monospace stack (`ui-monospace, SFMono-Regular, Menlo, Consolas, monospace`)
for machine values (SSID, IP). Render the small sidebar/card icons as **inline
SVG** (or simple Unicode glyphs where adequate) embedded in the markup; do not
fetch or embed webfont files.

**Rationale**: Satisfies FR-012/FR-020 (fully offline, no embedded fonts, small
pages) while approximating the design's monospaced-value aesthetic. Inline SVG
keeps icons crisp and asset-free.

**Alternatives considered**:
- Embed a webfont (WOFF2) in flash: rejected per clarification (size, offline
  simplicity).
- Icon font: rejected — that *is* an external font; inline SVG is lighter.

## RES-007 — Display-only Static-IP & decorative account chrome

**Decision**: Render the Static-IP toggle as visually "off"/disabled and the IP /
Subnet / Gateway fields as read-only, populated from the device's current
addressing (IP from `wifi_config_get_active_ip()`; subnet/gateway shown as the
typical class-C values or the live netif values if readily available, marked
read-only). Render the sidebar profile, header avatar, notification, and logout
elements as static, non-interactive markup using device-appropriate text
(e.g. "POV Display" / device SSID), with no form actions or links that mutate
state.

**Rationale**: Honors the "precise" visual match while adding no networking or
account behavior (spec Out of Scope; FR-018/FR-019). Read-only inputs / inert
elements cannot be submitted.

**Alternatives considered**:
- Omit these elements: rejected — reduces fidelity the user explicitly asked for.
- Make them functional: rejected — out of scope (new networking / accounts).

## RES-008 — Brightness range & safe floor

**Decision**: Expose brightness as 0–100% in the UI, stored as a `uint8_t`
(0–255 internal, or 0–100 stored then scaled). Clamp the *applied* value to a
safe minimum (e.g. never below a small floor unless explicitly 0) so the panel
does not appear "broken"; default brightness is full (matching today's behavior)
or the persisted value.

**Rationale**: Satisfies the brightness-bounds edge case (panel never stuck
unusably dark by accident) while still allowing a deliberate low setting.

**Alternatives considered**:
- Allow raw 0 silently: acceptable as an explicit user choice but guarded with a
  clear UI state; the floor applies to clamping logic, not to forbidding 0.

## Design Tokens (extracted from Figma frames `1:2` and `9:4`)

Captured via `get_design_context` for faithful CSS (mapped to CSS custom
properties; theme = dark default with a light override set).

| Token | Dark value | Usage |
|-------|-----------|-------|
| `--bg` | `#0d0f14` | app background |
| `--sidebar` | `#0a0c11` | sidebar background |
| `--card` | `#13161e` | metric/setting cards |
| `--inset` | `#1a1d27` | inputs, slider track |
| `--border` | `rgba(255,255,255,0.07)` | card/input borders |
| `--border-soft` | `rgba(255,255,255,0.06)` | sidebar dividers |
| `--accent` | `#2dd4bf` | brand/teal, active nav, links, scan border |
| `--accent-bg` | `rgba(45,212,191,0.1)` | active nav bg, avatars |
| `--danger` | `#ef4444` | Update Firmware button |
| `--text` | `#e2e4e9` | primary text/values |
| `--muted` | `#6b7280` | labels, descriptions |

Geometry / type:
- Radii: card `7px`, input/button `5px`, small chip `3.5px`, pill `999px`.
- Card padding `17.5px`; metric card padding `18.5px`; page padding `21px`;
  grid gap `21px`; row gap `14px`.
- Sidebar width `196px`; header height `49px`; settings column max-width `500px`.
- Uppercase mono labels: `~10.5px`, letter-spacing `~1.05px`, color `--muted`.
- Metric value: monospace `21px` / line-height `28px`, color `--text`.
- Fonts: design uses Inter (UI) + Space Mono/Cousine (mono) → mapped to system
  stacks `system-ui,...` (sans) and `ui-monospace,'Space Mono',Menlo,Consolas,
  monospace` (mono) per RES-006.
- Brightness slider shown at `78%`; theme toggle "Dark ⟷ Light" (dark active).
