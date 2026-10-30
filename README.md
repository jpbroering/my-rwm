<!--
SPDX-FileCopyrightText: © 2026 Isaac Freund
SPDX-FileCopyrightText: © 2026 João Paulo Broering
SPDX-License-Identifier: 0BSD
-->

# tinyRWM

Tiny River window manager implemented in C.

## Dependencies

The following system dependencies are required:

- pkg-config
- meson
- ninja
- wayland
- xkbcommon

The "development" versions are required if applicable to your distribution.

## Building

```sh
meson setup build
ninja -C build
```

## Running

```
river -c ./build/tinyrwm
```

## Current protocol lifecycle

The current River protocol interactions, build setup, and object ownership are
documented in [`docs/protocol-lifecycle.md`](docs/protocol-lifecycle.md).

Lifecycle reference and Wayland request regression tests are included in the
Meson test suite and can be run with `meson test -C build --print-errorlogs`.
They use the normal build dependencies and run without a live compositor.

## Current layout model

New windows automatically tile in a BSP layout, splitting the last focused tile
along its longer dimension. Each output owns nine independent workspace records;
workspace 1 is currently active. Output geometry and client minimum-size hints
feed the layout, and closing a window collapses its empty branch.

The independent model also implements directional focus, leaf swaps, and split
resizing. Workspace switching/moves, floating toggles, and the planned keyboard
actions are Step 3. New tiled windows ignore floating pointer move/resize requests.
Runtime configuration and IPC are later steps.

See [docs/model.md](docs/model.md) for ownership, layout rules, output removal
behavior, and model test coverage. No new build or runtime dependency is required.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for branch naming, incremental commits,
pull requests, validation, and release conventions.
