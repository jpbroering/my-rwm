<!--
SPDX-FileCopyrightText: © 2026 João Paulo Broering
SPDX-License-Identifier: 0BSD
-->

# River protocol lifecycle

This document records the protocol and ownership behavior of the current tinyRWM
implementation. It describes the baseline before workspace and BSP state are
introduced.

## Build and protocol setup

Meson uses `wayland-scanner` through its Wayland module to generate client
bindings from `protocol/river-window-management-v1.xml` and
`protocol/river-xkb-bindings-v1.xml`. The executable links against
`wayland-client` and `xkbcommon`; no separate generated-code step is needed.

At startup tinyRWM binds `river_window_manager_v1` at version 4 and
`river_xkb_bindings_v1` at version 1. Version 4 is required by the current
listener/request set. A missing global or an unsupported version stops startup.

## Manage and render sequences

River sends object events and then `manage_start`. tinyRWM removes objects marked
closed or removed, processes initial window dimensions and input requests,
updates focus and bindings, then sends `manage_finish`. Window-management state
requests are confined to this phase.

Each `render_start` applies pending pointer movement or resize positioning and
ends with `render_finish`. Output position and dimensions events are currently
ignored, and there is no workspace or tile model yet.

## Object ownership

| Object | Local owner | Destruction point |
| --- | --- | --- |
| `river_window_manager_v1` | Process | After `finished`, or immediately on `unavailable` |
| `river_xkb_bindings_v1` | Process | After manager shutdown |
| `river_window_v1` and its `river_node_v1` | `Window` in `wm.windows` | On `closed`, or after manager `finished`; destroy the node before the window |
| `river_output_v1` | `Output` in `wm.outputs` | On `removed`, or after manager `finished` |
| `river_seat_v1` | `Seat` in `wm.seats` | On `removed`, or after manager `finished`; destroy its bindings first |
| XKB/pointer binding objects | Their owning `Seat` | Before the owning seat is freed |

River object user data points to the corresponding local `Window` or `Seat` so
events that carry protocol objects can be mapped back to manager state. A window
close clears all seat references to it, ends any live pointer operation on a
remaining seat, clears its user data, destroys its node and window proxies, and
then frees the model object. Seat removal clears window pointer-request
references before destroying the seat and its bindings. Manager shutdown
destroys all remaining child objects before releasing the manager and XKB
globals.

The seat/window reference cleanup helpers are independent of Wayland, so the
test suite can exercise them without a compositor session.
