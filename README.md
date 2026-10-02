<!--
SPDX-FileCopyrightText: © 2026 Isaac Freund
SPDX-License-Identifier: 0BSD
-->

# My River Window Manager

This project is based on the Tiny river window manager implemented in C.

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
