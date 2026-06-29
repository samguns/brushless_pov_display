# Feature Specification: Management Portal UI/UX Redesign

**Feature Branch**: `007-portal-ui-redesign`

**Created**: 2026-06-29

**Status**: Draft

**Input**: User description: "I'd like to polish UI/UX according to this figma design https://www.figma.com/design/qyddA8TwStquAD5TYAcYhC/pov-mgmt , cooperate with figma skill to implement it"

## Overview

The device already serves a small management portal (a status page, a "Change
Wi-Fi" page, and firmware-update pages) over the local network. The portal works
but looks plain. The referenced Figma file (`pov-mgmt`) provides a modern,
dark-themed "POV Display" admin layout with a left navigation sidebar, an
**Overview** dashboard of metric cards, and a **Settings** screen grouped into
Display, System, and Network cards.

This feature is primarily a **visual and interaction polish**: re-skin and
re-organize the existing portal so it matches the look and feel of the Figma
design, while continuing to surface the same real device information and the same
Wi-Fi-change and firmware-update actions the portal already supports. Beyond
presentation, it also implements the two display-preference controls shown in the
design — a Dark/Light theme switch and an LED-panel brightness control — so the
Settings screen matches the design precisely and functionally. It otherwise adds
no new device capabilities (no new radio or storage subsystem).

## Clarifications

### Session 2026-06-29

- Q: How should the Display card (Theme toggle + Brightness slider) be handled, given no current device backing? → A: Implement for real — client-side Dark/Light theme switching plus a Brightness control that adjusts the actual LED-panel output brightness.
- Q: How should the Network card's Static IP toggle and IP/Subnet/Gateway fields be handled, given the device uses DHCP with no static-IP support? → A: Render them for visual fidelity in a display-only/disabled state (toggle off, fields read-only) reflecting current addressing; functional static-IP configuration is not built.
- Q: How should the design's account chrome (sidebar profile, notification bell, logout, header avatar) be handled, given there is no user-account system? → A: Render the chrome visually with device-appropriate static content; the elements are non-functional/decorative.
- Q: Should the Overview metric cards show live-updating values or values as of page load? → A: Values as of page load with manual refresh (matches current behavior); no auto-polling required.
- Q: How precisely should typography match given the no-external-asset constraint? → A: Approximate with a self-contained system font stack (sans UI + monospaced values); do not embed or fetch external font files.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Modern Overview dashboard (Priority: P1)

The device owner opens the portal in a browser and lands on an **Overview**
screen that matches the Figma design: a dark layout with a left sidebar
("POV Display" brand, Overview / Settings navigation) and a row of metric cards
showing the live device state — connection status, the connected network (SSID),
and the IP address — with the device's blink state also visible.

**Why this priority**: The status/overview screen is the first and most frequent
thing the owner sees. Delivering just this redesigned landing screen already
makes the portal feel modern and is independently usable.

**Independent Test**: Load the portal root in a browser while the device is
connected; confirm the Overview screen renders with the sidebar and metric cards
and that each card shows the device's actual current values.

**Acceptance Scenarios**:

1. **Given** the device is connected to a network, **When** the owner opens the
   portal root, **Then** an Overview screen is shown with a left sidebar and
   cards displaying Status ("Connected"), Network (the real SSID), and IP
   Address (the real IP), styled to match the Figma Overview frame.
2. **Given** the device reports blink state and frequency, **When** the Overview
   screen renders, **Then** the current blink activity and frequency are visible
   on the screen alongside the other metrics.
3. **Given** a recent Wi-Fi change or firmware action produced a notice, **When**
   the Overview screen renders, **Then** that notice is shown as a clearly styled
   banner consistent with the design.

---

### User Story 2 - Redesigned Settings: change Wi-Fi (Priority: P2)

From the sidebar the owner opens **Settings** and finds a **Network** card
styled per the Figma design: the current SSID, a way to pick from nearby
networks, an explicit **Scan** action, and a password field. The owner can
change the device's Wi-Fi network from this redesigned screen exactly as they
can today.

**Why this priority**: Changing Wi-Fi is the portal's primary configuration
action. Re-housing it in the new Settings layout preserves existing function
while completing the redesign of the most important interactive flow.

**Independent Test**: Open Settings, trigger a scan, pick or type an SSID, enter
a password, submit, and confirm the device attempts the change and the
in-progress / result feedback is shown in the new visual style.

**Acceptance Scenarios**:

1. **Given** the owner is on the Settings screen, **When** they activate **Scan**,
   **Then** nearby networks are listed and one can be selected to fill the SSID
   field, styled per the design.
2. **Given** the owner has entered an SSID and a valid password, **When** they
   submit the Network card, **Then** the device begins switching networks and an
   "applying / switching" state is shown in the new visual style.
3. **Given** the owner submits an invalid SSID or password, **When** the form is
   processed, **Then** an inline validation message is shown in the redesigned
   style and no network change is attempted.

---

### User Story 3 - Redesigned Settings: firmware update (Priority: P3)

On the same **Settings** screen the owner sees a **System** card showing the
firmware version and an **Update Firmware** button (styled as the prominent /
destructive action in the design). Activating it leads to the same confirmation
and update flow the portal already provides, re-skinned to match the design.

**Why this priority**: Firmware update is an occasional but important action.
It rounds out the Settings redesign but is less frequently used than status or
Wi-Fi changes.

**Independent Test**: Open Settings, locate the System card, activate Update
Firmware, and confirm the confirmation page and subsequent flow appear in the
new visual style and behave as before.

**Acceptance Scenarios**:

1. **Given** the owner is on Settings, **When** the System card renders, **Then**
   it shows the firmware version and an Update Firmware action styled per the
   design.
2. **Given** the owner activates Update Firmware, **When** the confirmation
   screen appears, **Then** it presents the existing warning and confirm/cancel
   choices re-skinned to match the design.

---

### User Story 4 - Consistent navigation and responsive layout (Priority: P4)

Every portal screen shares the same sidebar/navigation shell so the owner can
move between Overview and Settings, and the layout adapts gracefully to a phone
screen (the portal is typically opened from a phone on the local network).

**Why this priority**: Shared navigation and mobile usability make the redesign
feel cohesive, but the individual screens are usable even before this is fully
polished.

**Independent Test**: On both a desktop-width and a phone-width browser, confirm
the navigation lets the owner switch between Overview and Settings and that
content remains readable and usable without horizontal scrolling.

**Acceptance Scenarios**:

1. **Given** any portal screen, **When** it renders, **Then** the owner can
   navigate to Overview and to Settings from a consistent navigation element.
2. **Given** a narrow (phone-width) viewport, **When** any screen renders,
   **Then** content stacks/reflows so it stays readable and all actions remain
   reachable without horizontal scrolling.

---

### User Story 5 - Display preferences: theme & brightness (Priority: P3)

On the Settings screen the owner sees a **Display** card matching the Figma
design with a Dark/Light theme toggle and a Brightness control. Switching the
theme restyles the portal; adjusting brightness changes the actual brightness of
the LED panel output.

**Why this priority**: These controls complete the precise visual match of the
Settings screen and give the owner real, useful control (especially LED
brightness). They are valuable but secondary to status, Wi-Fi change, and
firmware update.

**Independent Test**: On Settings, toggle the theme and confirm the portal
restyles between dark and light; move the brightness control and confirm the LED
panel's output brightness changes and the setting is retained after a reboot.

**Acceptance Scenarios**:

1. **Given** the owner is on Settings with the default dark theme, **When** they
   switch the theme to Light, **Then** the portal restyles to the light
   appearance (and back to dark when toggled again).
2. **Given** the owner is on Settings, **When** they change the Brightness
   control, **Then** the LED panel's output brightness changes accordingly.
3. **Given** the owner has set a brightness level, **When** the device reboots,
   **Then** the previously selected brightness is restored.

---

### Edge Cases

- **No networks found on scan**: the Network card shows a clear "no networks
  found, enter SSID manually" message in the new style rather than an empty list.
- **Long SSID / values**: long SSIDs, IPs, or notices must not break the card
  layout (they wrap or truncate cleanly).
- **Disconnected / unknown state**: when the device is not connected, the
  Overview status card communicates that state clearly rather than showing a
  blank or misleading "Connected".
- **Connection drops during a Wi-Fi change**: the "applying" screen already
  warns the connection will drop; the redesigned screen must preserve that
  guidance so the owner knows to reconnect.
- **Page size limits**: the redesigned pages must still be generated within the
  device's fixed page-rendering budget (see Assumptions); richer styling must not
  cause pages to be truncated or fail to render.
- **No external assets**: every screen must render fully even though the device
  serves pages by itself with no internet access (no externally hosted fonts,
  images, scripts, or stylesheets).
- **Brightness bounds**: the brightness control must clamp to a safe range so it
  cannot drive the LED panel to an invalid or fully-unusable (e.g. 0) state in a
  way that makes the device appear broken.
- **Static-IP fields are non-editable**: the Static IP toggle and the
  IP/Subnet/Gateway fields are present for visual fidelity but read-only; the
  owner cannot submit changes to them and no networking change results.
- **Account chrome is inert**: the profile, notification, and logout elements are
  decorative; activating them performs no account/session action.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The portal MUST present an Overview screen visually consistent with
  the Figma `Overview` frame, including a left navigation sidebar with the
  "POV Display" brand and Overview / Settings navigation entries.
- **FR-002**: The Overview screen MUST display the device's real connection
  status, connected network (SSID), and IP address as distinct metric cards.
- **FR-003**: The Overview screen MUST display the device's blink state and blink
  frequency.
- **FR-003a**: The Overview screen MUST present its values as of page load; live
  auto-refresh/polling is NOT required (reloading the page updates the values).
- **FR-004**: The portal MUST present a Settings screen visually consistent with
  the Figma `settings-screen` frame, organized into grouped Display, System, and
  Network cards.
- **FR-005**: The Settings screen MUST include a Network card that lets the owner
  view the current SSID, scan for nearby networks, select a discovered network to
  fill the SSID, enter a password, and submit a Wi-Fi change — preserving the
  existing change/validation/apply/revert behavior.
- **FR-006**: The Settings screen MUST include a System card that shows the
  firmware version and provides an Update Firmware action that leads to the
  existing confirmation-and-update flow.
- **FR-007**: The Update Firmware action MUST be visually emphasized as a
  prominent/destructive action consistent with the design.
- **FR-008**: All portal screens MUST share a consistent navigation shell that
  lets the owner move between Overview and Settings.
- **FR-009**: All portal screens MUST be readable and fully operable on a
  phone-width viewport without horizontal scrolling.
- **FR-010**: The portal MUST adopt the design's visual language consistently
  across screens (dark theme, accent color, card-based grouping, muted uppercase
  labels with prominent values, and monospaced presentation of machine values
  such as SSID and IP).
- **FR-011**: Recent action outcomes (Wi-Fi change result, errors, informational
  notices) MUST be shown as a clearly styled banner/inline message consistent
  with the design.
- **FR-012**: Every screen MUST render correctly with no externally hosted
  assets (the device serves all needed markup/styling itself, offline).
- **FR-013**: The redesign MUST NOT change the existing portal's behavior,
  endpoints' effects, validation rules, or security posture for the status,
  Wi-Fi-change, and firmware-update actions — only their presentation.
- **FR-014**: The redesigned pages MUST be produced within the device's fixed
  page-rendering memory budget without truncation (see Assumptions); any required
  increase to that budget MUST be explicitly accounted for.
- **FR-015**: The Settings screen MUST include a Display card matching the design,
  containing a Dark/Light theme toggle and a Brightness control.
- **FR-016**: The theme toggle MUST switch the portal's appearance between dark
  and light, with dark as the default. It operates on the client side and does
  not require device-side storage (the preference may be remembered by the
  browser).
- **FR-017**: The Brightness control MUST adjust the actual brightness of the LED
  panel output, clamped to a safe range, and the selected level MUST persist
  across device reboots.
- **FR-018**: The Network card MUST render the Static IP toggle and the IP
  Address / Subnet Mask / Gateway fields for visual fidelity in a
  display-only/read-only (non-editable) state reflecting the device's current
  network addressing; functional static-IP configuration is NOT provided.
- **FR-019**: The portal MUST render the design's account chrome (sidebar profile
  area, header avatar, notification and logout affordances) using
  device-appropriate static content; these elements are non-functional and
  perform no account/session actions.
- **FR-020**: Typography MUST use a self-contained system font stack (sans-serif
  for UI text, monospaced for machine values such as SSID/IP); external font
  files MUST NOT be embedded or fetched.

### Key Entities *(data displayed, not new data)*

- **Device status**: connection state, connected SSID, IP address, blink active
  flag, blink frequency — all already available from the running firmware.
- **Nearby network**: an SSID and whether it is secured — already produced by the
  existing scan capability, presented as selectable list items.
- **Action notice**: a short human-readable message describing the result of a
  recent Wi-Fi change or other action, shown as a banner.
- **Display preferences**: the UI theme (Dark default / Light, client-side) and
  the LED-panel brightness level (a value within a safe range, persisted on the
  device across reboots).
- **Read-only network addressing**: the current IP address, subnet mask, and
  gateway shown for fidelity in the Network card; not editable.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A reviewer comparing the rendered Overview and Settings screens
  side-by-side with the Figma frames judges them a faithful match in layout,
  grouping, color/theme, and typography (no element from the existing plain pages
  remains unstyled).
- **SC-002**: 100% of the real device values currently shown in the portal
  (status, SSID, IP, blink state, blink frequency, firmware version) appear in the
  redesigned screens.
- **SC-003**: The owner can complete a Wi-Fi network change end-to-end from the
  redesigned Settings screen in the same number of steps or fewer than the
  current portal, with no regression in success rate.
- **SC-004**: The owner can reach and trigger the firmware-update confirmation
  from the redesigned Settings screen.
- **SC-005**: On a phone-width viewport (≤ 420 px wide), every screen is readable
  and every action is reachable with no horizontal scrolling.
- **SC-006**: Each redesigned screen renders fully with the device offline (no
  external network requests are required for any screen to display correctly).
- **SC-007**: All redesigned pages render completely (no truncation) within the
  device's fixed page-rendering budget.
- **SC-008**: Toggling the theme switches the entire portal between dark and
  light appearance, with dark shown by default on first load.
- **SC-009**: Adjusting the Brightness control changes the LED panel's actual
  output brightness, and the selected level is retained after a device reboot.
- **SC-010**: The Static IP toggle and the IP/Subnet/Gateway fields are visible
  (matching the design) but cannot be edited or submitted.
- **SC-011**: A reviewer comparing the rendered Settings screen with the Figma
  `settings-screen` frame finds the Display, System, and Network cards all
  present and faithful in layout, grouping, and styling.

## Assumptions

- **Scope is visual/interaction polish of existing functionality, plus two
  display-preference controls.** The redesign re-skins and re-organizes what the
  portal already does, and additionally implements the Theme and Brightness
  controls shown in the design (see Clarifications).
- **Real-data mapping over decorative template content.** The Figma file is a
  general admin template; placeholder/sample elements that are not backed by
  real device data are treated as design language to imitate, not features to
  build. The Overview frame's chart/table/activity layers are hidden in the
  design and are therefore not part of the precise visual match.
- **Theme & brightness are implemented; static-IP is display-only.** Per
  clarification: the theme toggle is client-side (dark default), the brightness
  control drives the actual LED-panel output and persists across reboots, and the
  Static IP toggle + IP/Subnet/Gateway fields are rendered read-only for fidelity
  (functional static-IP configuration is not built).
- **Account chrome is decorative.** The sidebar profile, header avatar,
  notification, and logout elements are rendered with device-appropriate static
  content and perform no actions.
- **Typography uses a system font stack** (no embedded/fetched webfonts), so text
  approximates the design while keeping pages small and fully offline.
- **The portal continues to be served by the device itself** as self-contained
  pages with no external/CDN assets and minimal client-side scripting, consistent
  with how it works today.
- **The device's fixed page-rendering buffer** is the memory budget referenced by
  FR-014/SC-007; if the richer markup requires enlarging it, the increase is
  bounded, documented, and statically allocated (no dynamic allocation).
- **Existing endpoints and flows are reused** (status, scan, Wi-Fi change,
  firmware update); their inputs, validation, and outcomes are unchanged.
- **Primary client is a phone or laptop browser** on the same local network as
  the device.

## Out of Scope

- Analytics/area/bar charts, recent-transactions table, and activity feed from
  the Figma dashboard (these layers are hidden in the design).
- Functional account / sign-out / notifications behavior and global search
  (the account chrome is rendered as decorative static content only).
- **Functional** static IP / subnet / gateway configuration — these fields are
  rendered read-only for visual fidelity, but the device remains on its current
  (DHCP) network-addressing behavior.
- Any change to underlying Wi-Fi, scanning, storage, or firmware-update behavior
  beyond presentation. (LED-panel brightness control is explicitly **in** scope
  per the clarification.)
