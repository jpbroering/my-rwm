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

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for branch naming, incremental commits,
pull requests, validation, and release conventions.
