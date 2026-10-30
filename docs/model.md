<!--
SPDX-FileCopyrightText: © 2026 João Paulo Broering
SPDX-License-Identifier: 0BSD
-->

# Core model and BSP layout

`model.h` and `model.c` implement the manager's state and layout without Wayland
dependencies. Callers own `ModelOutput` and `ModelWindow` storage. The model owns
each output's workspace array and each workspace's BSP nodes.

## Ownership and window state

An output has its logical bounds, a workspace count supplied at initialization,
and an active workspace index. Workspace numbers start at 1; each workspace has
its own window list, BSP root, focused window, and last focused tiled window.
The runtime currently initializes nine workspaces per output and uses workspace
1. Configuration of the count is part of Step 4; workspace commands are Step 3.

A window has at most one workspace. Tiled windows occupy exactly one BSP leaf;
floating windows belong to the workspace list without occupying a leaf. Model
rectangles describe requested tile geometry or the last floating geometry. The
River adapter separately caches actual client dimensions, because clients may
decline requested sizes. Minimum width/height hints are normalized to at least
one pixel; maximum-size hints do not constrain the BSP tree.

Initialize windows with `model_window_init` and outputs with `model_output_init`
using zero-initialized output storage. Add detached windows with
`model_workspace_add`. Allocate both new nodes before changing the tree so an
insertion failure leaves the old layout and window membership intact. Set
floating state before insertion; changing an attached window's mode directly
would violate the model invariants. Floating toggle/restore behavior is Step 3.

`model_workspace_remove` collapses an empty branch, updates list links and focus,
and clears the window's workspace, tile, and list references. Removing the focused
tiled window prefers a leaf from its surviving sibling branch. Output teardown
detaches all windows and frees all workspace/tree allocations; the caller's
window storage remains valid. Windows must be removed before freeing their
storage. An initialized output must be finished before its storage is moved or
reused, since workspaces refer back to it.

## Deterministic layout rules

- A new tile splits the last focused tiled leaf at 50:50. The longer dimension
  determines the split axis; squares split into left/right children. The existing
  window keeps the first child and the new window takes the second child. A
  floating focus preserves the last focused tile as the insertion target.
- Ratios use integer fractions with denominator 2^32. Layout rounds the first
  child's extent down and gives the remainder to the second child, covering the
  parent's rectangle exactly. Output origins may be negative.
- Subtree minimum extents add along the split axis and take the maximum along
  the other axis. Layout clamps each cut to both children's minima when they
  fit. When they do not fit, it divides space proportionally to those minima,
  keeping both children nonempty whenever there are at least two pixels. Zero
  or one available pixel can produce empty tiles; extents never become negative.
  The adapter proposes at least 1x1 to River, whose clients cannot use zero sizes.
- Minimum-size clamping preserves stored ratios, so a temporary output shrink
  does not discard the intended layout proportions. Invalid output dimensions
  or coordinates that would overflow a rectangle are rejected.
- Directional focus searches only the current workspace's tiled leaves, using
  centers in the requested half-plane. It selects the smallest Manhattan center
  distance, then the smallest perpendicular distance, then first-child tree
  traversal order. Distances use integer arithmetic, including half-pixel centers.
  Floating windows and other workspaces/outputs are excluded. A missing candidate
  leaves focus unchanged.
- Swapping exchanges the windows in two leaves, preserving all nodes and ratios.
  Focus stays with the window. Layout then reapplies the occupants' minimum hints.
- Resizing grows the requested edge by a caller-supplied pixel step. It adjusts
  the nearest ancestor split that forms that edge, clamps against subtree minima,
  and saves the resulting ratio. Missing boundaries, zero steps, and already
  clamped splits leave the layout unchanged.

## River adapter

Output position/dimension events and window minimum-size hints feed the model.
During `manage_start`, tinyRWM removes dead objects, applies pending interaction
focus to the model, and attaches new windows to the focused window's output (or
the first output when there is no focused output). It proposes BSP dimensions
and tiled edges during the manage sequence and applies tile positions during
the render sequence.

When an output disappears, its surviving windows are detached and inserted into
the first surviving output's active workspace, unless a remaining seat focus
selects another output. If no output remains, windows stay detached until an
output appears. Hints and floating state survive this reassignment.

New windows are tiled by default. Pointer move/resize operations require floating
state so they cannot overwrite tile geometry. Directional model operations,
workspace switching, and floating toggles are not yet exposed as keyboard actions;
that is Step 3. Runtime TOML configuration and IPC remain Steps 4 and 5.

## Tests

`meson test -C build --print-errorlogs` includes isolated model tests for outputs,
workspace separation, splits, directional focus, swapping, resizing, removal,
minimum sizes, degenerate/extreme bounds, and 2,000 deterministic state transitions
with tree/list/focus checks after every operation.

Wayland request tests additionally verify automatic tiling, output geometry and
hint changes, focused insertion, close collapse, output removal/reappearance,
and rejecting pointer operations on tiled windows. These tests use the real
adapter callbacks and generated request encoding over a socket pair. A live
River integration run is still part of Step 6; no new dependency is required.
