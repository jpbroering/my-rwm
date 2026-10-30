# Repository guidance

## Project context

tinyRWM is a C window manager for River. Read [`PLAN.md`](PLAN.md) before implementing behavior; it records the intended user-facing requirements and current design direction. Treat the plan as a target, not as evidence that a feature is already implemented.

## Existing project conventions

- The project uses Meson and C99. Keep build changes in `meson.build` and follow its existing dependency and source-generation patterns.
- Preserve the project's SPDX headers and license notices when adding or editing source files.
- Match the surrounding C style and keep River/Wayland protocol object lifetimes explicit.
- River is the compositor. Keep tinyRWM focused on window management; do not add a bundled status bar or application launcher.

## Implementation expectations

- Keep runtime settings outside compiled C code. Key bindings and visual settings belong in separate editable configuration files, with sensible defaults when files are absent.
- Keep workspace, tile, and floating-window state in the manager's model. Do not assume River supplies workspace or BSP state.
- Clear all references to a window or seat before freeing it; account for close, removal, and protocol teardown paths.
- Keep external control documented and versioned. Do not expose private in-memory pointers or rely on undocumented implementation details in the IPC contract.
- Add automated coverage as behavior is implemented. Layout algorithms and state transitions should be testable independently of a live River session where possible; add protocol integration coverage for compositor interactions.
- Document new build/runtime dependencies and user-visible configuration or IPC changes.

## Working in this repository

- Inspect the current working tree before editing. Preserve unrelated or untracked user work.
- Keep changes focused on the requested behavior and update `PLAN.md` or `README.md` when implementation changes make their descriptions inaccurate.
- Do not claim a build or test passed unless it was actually run and completed successfully.

## Version control

- Read and follow [`CONTRIBUTING.md`](CONTRIBUTING.md) before implementation or Git operations. It defines branch naming, incremental commits, pull requests, merges, and releases.
- Implementation and documentation requests authorize local task branches and commits unless the user requests otherwise. Commit each coherent working increment as implementation progresses, including its tests and documentation.
