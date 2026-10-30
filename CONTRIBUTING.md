<!--
SPDX-FileCopyrightText: © 2026 João Paulo Broering
SPDX-License-Identifier: 0BSD
-->

# Contributing to tinyRWM

Read [PLAN.md](PLAN.md) for the intended behavior and [AGENTS.md](AGENTS.md) for
implementation guidance. The conventions below apply to contributors and coding
agents.

## Branches

Use a simple GitHub flow: `main` holds accepted, buildable development; temporary
topic branches hold individual changes. Releases are identified by tags.

| Name | Purpose | Example |
| --- | --- | --- |
| `main` | Accepted development | `main` |
| `feature/*` | New behavior | `feature/bsp-layout` |
| `fix/*` | Bug fixes | `fix/resize-seat-removal` |
| `refactor/*` | Restructuring that preserves behavior | `refactor/window-ownership` |
| `docs/*` | Documentation and conventions | `docs/git-conventions` |
| `test/*` | Test coverage | `test/output-removal` |
| `build/*` | Build tooling and dependencies | `build/meson-options` |
| `ci/*` | Continuous integration | `ci/sanitizers` |

- Use lowercase words separated by hyphens after the prefix. Describe the change
  in the name; put plan step numbers and progress status in issues or PRs.
- Start independent work from current `main`. Keep a branch focused on one
  reviewable change; a large plan step can span several branches.
- Continue an existing branch when the task belongs to that change. If a new
  branch requires unmerged work, make the dependency explicit and initially
  target the parent branch in its PR; retarget `main` after the parent merges.
- Inspect the working tree before switching branches and preserve unrelated
  edits and untracked files.
- When `main` advances, merge it into active shared branches as needed. Preserve
  published history and contributor authorship. Rebasing shared branches,
  amending others' commits, and force-pushing require explicit authorization.
- Delete completed branches after a successful merge within the authorized
  workflow. Create a fresh branch for subsequent work.

## Commit during implementation

Implementation and documentation requests authorize local task branches and
commits unless the user asks for a patch or uncommitted changes.

- Complete a coherent working increment, run relevant checks, inspect its diff,
  and commit it before starting the next increment. Include the corresponding
  tests and documentation with each behavior change.
- Before reporting a task complete, commit all changes belonging to it. Report
  any intentionally uncommitted work and any checks that could not run.
- Stage explicit paths or selected hunks. Inspect the staged diff before every
  commit, including new files. Exclude unrelated work, generated build files,
  editor state, and credentials.
- Use Conventional Commit subjects: `feat:`, `fix:`, `refactor:`, `test:`,
  `docs:`, `build:`, `ci:`, or `chore:`. An optional scope may identify the
  affected component, such as `fix(lifecycle): end resize when a seat disappears`.
- Describe the resulting change. Keep the body for motivation, compatibility
  consequences, or validation details that help future readers.

## Validation and pull requests

For C or build changes, use the Meson build and relevant automated tests:

```sh
meson setup build
meson compile -C build
meson test -C build --print-errorlogs
```

Reuse an existing build directory when it is already configured. Exercise new
behavior and important boundaries using the existing suites. For documentation
changes, review the text, relative links, and `git diff --check`; a full C build
is not required. Record checks actually completed and their limitations.

- Publishing branches, opening PRs, merging, and publishing releases must fit
  the user's authorized workflow. Carry existing authorization forward.
- Open one focused PR per reviewable change, normally targeting `main`. Link
  existing issues or the relevant plan step when applicable.
- Describe the resulting behavior, relevant design decisions, validation, and
  remaining limitations. Keep the title and description aligned with the final
  scope.
- Resolve review findings and applicable required checks before merging. Read
  check results before performing the merge. Validate the actual final candidate;
  earlier green results do not cover materially changed code.
- Use a merge commit (`--no-ff`) to retain the implementation commits and feature
  boundary. Resolve conflicts by inspecting both sides and the intended behavior.
- After an authorized push, verify the remote branch/PR state and applicable CI
  on the pushed commit. Pending or skipped checks are not passing checks.

## Versions and releases

- Use `MAJOR.MINOR.PATCH` in `meson.build` for releases, with matching annotated
  tags named `vMAJOR.MINOR.PATCH`. Existing development versions are normalized
  when preparing a release.
- Change versions during release preparation. A commit, branch, or completed
  plan step does not automatically require a version bump.
- From 1.0 onward, use PATCH for compatible fixes, MINOR for compatible additions,
  and MAJOR for breaking public contracts. During `0.x`, use MINOR for features
  or breaking changes and PATCH for compatible fixes.
- Record configuration, CLI, and IPC compatibility changes in release notes.
  Version the IPC contract independently of the application release.
- Release from an accepted `main` commit. Introduce `stable/X.Y` branches only
  when maintaining older release series alongside ongoing development. Backport
  selected fixes with their source commit recorded; keep unreleased features
  out of maintenance releases.
- A release requires an explicit request. Commit version/release metadata first,
  validate that final candidate, then tag and publish that exact commit. Verify
  applicable CI on that commit, or document the local gate when hosted CI is
  absent. Published tags are immutable.

## References

- [GitHub flow](https://docs.github.com/en/get-started/using-github/github-flow)
- [GitHub contribution guidelines](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/setting-guidelines-for-repository-contributors)
