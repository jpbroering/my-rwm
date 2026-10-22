# tinyRWM project plan

## Purpose

Build a small, configurable C window manager for the River Wayland compositor, using River's window-management protocol. The local repository is the starting point; this plan describes the intended behavior and implementation direction, not features that are already complete.

## Product requirements

- Automatically tile new windows. Adding a window replaces the focused tile with two children split initially at 50:50. Choose the split axis from the focused tile's shape (split along its longer dimension); respect minimum sizes where possible.
- Support multiple numbered workspaces, independently available on each output. A workspace owns its windows and layout. Switching workspaces hides the old workspace's windows and shows the selected workspace's windows.
- Move the focused window to another workspace by shortcut, preserving whether it is tiled or floating.
- Use no window-manager decorations by default: no titlebars or frames, and a border size of zero. Border size and colors remain configurable. A status bar is not part of tinyRWM.
- Let other programs, including Waybar and Quickshell, inspect state and control the manager through a documented interface.
- Load configuration at runtime from human-editable files; changing configuration must not require recompiling tinyRWM.
- Keep key bindings separate from appearance settings. Appearance settings include border size and color, tile spacing, and screen margins.
- Provide automated tests for layout behavior and window-management actions.

## Window and layout behavior

### Tiled windows

- A new tiled window splits the focused tile into equal children. Clamp the split only as needed to respect client minimum-size hints and available output space.
- Each workspace has its own binary space partitioning (BSP) tree. A leaf represents one tiled window; an internal node represents a horizontal or vertical split and its ratio.
- Directional focus selects the nearest tiled window in that direction. If there is no candidate, focus stays where it is.
- Moving a tiled window between tiles swaps the windows occupying the two leaves, leaving the BSP tree and split ratios in place. If there is no adjacent tile, the action does nothing.
- Resizing a tile split adjusts the nearest relevant split in the requested direction by a configurable step, clamped so the split's child tiles remain within their minimum sizes where possible.
- Removing a tiled window collapses its now-empty branch and focuses a remaining visible window.

### Floating windows

- A tiled window can be toggled to floating and back. Keep its last floating rectangle and saved tile position; restore that tile position when possible, otherwise insert it by splitting the currently focused tile.
- Super + left-button drag moves a floating window. Super + right-button drag resizes it.
- Floating windows remain associated with their workspace and retain floating mode when moved between workspaces.

### Workspaces and lifecycle

- Provide workspaces 1–9 by default; keep the count configurable.
- Super + 1–9 switches the focused output to that workspace. Super + Shift + 1–9 moves the focused window to that workspace.
- On window close, remove the window from the workspace/layout and update focus safely. Window and seat references must be cleared before their objects are freed.

## Default controls

Bindings are defaults only; users can change them in the key binding configuration.

| Action | Default |
| --- | --- |
| Focus adjacent tile | Super + arrow key |
| Swap focused window with adjacent tile | Super + Shift + arrow key |
| Resize nearest tile split | Super + Ctrl + arrow key |
| Switch to workspace 1–9 | Super + 1–9 |
| Move focused window to workspace 1–9 | Super + Shift + 1–9 |
| Close focused window | Super + W |
| Run configured application launcher | Super + Space |
| Toggle tiled/floating mode | Super + Shift + Space |
| Move floating window | Super + left-button drag |
| Resize floating window | Super + right-button drag |

The launcher itself is an external program. Configure its command in tinyRWM; do not bundle a launcher or status bar.

## Configuration

- Store configuration under `$XDG_CONFIG_HOME/tinyrwm/` (falling back to `~/.config/tinyrwm/`).
- Use TOML files with separate responsibilities, for example `keybinds.toml`, `appearance.toml`, and `settings.toml`.
- Supply working built-in defaults when files are absent. Validate parsed values and report file/line context for errors.
- Support reload while the manager is running, through the control interface and optionally SIGHUP. If a reload fails, keep the last valid configuration active.
- Keep the parser and any new dependency small and documented. Confirm availability and maintenance before choosing a C TOML library.

## External control interface

Use a local Unix-domain socket, normally at `$XDG_RUNTIME_DIR/tinyrwm.sock`, with a versioned JSON-lines protocol. Provide a small `tinyrwmctl` client for interactive use and scripts.

The interface should support:

- Querying the active output/workspace, focused window, window identifiers, floating/tiled state, and current layout state.
- Commands for focus, workspace switching, moving windows, toggling floating, closing, and reloading configuration.
- Events for workspace changes, focus changes, window creation/close, and layout changes so status bars do not need to poll continuously.

Document message schemas, error responses, and version negotiation. Restrict the socket to the current user and avoid exposing pointers or unstable internal identifiers as a public API. Keep River-specific protocol details behind the manager's internal model.

## Technical direction and alternatives

| Concern | Alternatives considered | Direction |
| --- | --- | --- |
| Automatic layout | BSP; master/stack; monocle | BSP, because each new window can split the focused tile and split boundaries can be resized independently. |
| Workspace ownership | River-managed tags; manager-owned workspaces | Manager-owned workspaces, since River's window-management protocol does not provide a workspace model. |
| External communication | Unix socket; D-Bus; custom Wayland protocol | Versioned Unix socket with JSON-lines, which is simple for scripts, Waybar, and Quickshell to consume. |
| Configuration | C header/recompile; ad hoc text; TOML | Runtime TOML files split by purpose, with validated defaults and reload. |
| Status bar and launcher | Bundle them; integrate external programs | Keep both external; expose state/actions through IPC and configure a launcher command. |

## Test and delivery plan

Add tests as the corresponding components are implemented; do not defer all coverage until the end.

1. **Baseline and protocol lifecycle:** document the current River protocol interactions, build setup, and object ownership; fix resource and stale-reference issues before expanding state management.
2. **Core model:** implement outputs, workspaces, window state, BSP nodes, and deterministic layout calculations. Add unit tests for split creation, focus direction, swapping, split resizing, branch collapse, and minimum-size constraints.
3. **Window behavior:** implement workspace switching/moves, floating toggles, pointer move/resize, close handling, and keyboard actions. Test state transitions and focus changes.
4. **Configuration:** add TOML loading, defaults, validation, split configuration files, and reload behavior. Test valid, missing, malformed, and out-of-range values, including keeping the prior configuration after a failed reload.
5. **IPC and tools:** implement the socket protocol, `tinyrwmctl`, state queries, commands, and events. Test protocol version/errors, permissions, and client interactions.
6. **Integration and CI:** run protocol-level integration tests with a test compositor or nested River session where practical; run build and automated tests in CI, with compiler warnings and sanitizers where supported.

## Completion criteria

- All product requirements above are implemented and documented.
- Users can change key bindings and appearance without rebuilding, and configuration reload errors do not discard the active valid settings.
- Waybar/Quickshell or another client can query state and receive events/use commands through the documented IPC.
- Automated unit and integration coverage exercises the layout, workspace, floating, focus, close, and IPC paths.
- The default installation includes neither a status bar nor a launcher, and uses no window-manager border by default.
