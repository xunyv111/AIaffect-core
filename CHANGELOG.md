# Changelog

All notable changes to Affect Core are documented here.

## 0.1.0-beta.2 - 2026-10-01

### Added

- Engine-neutral TypeScript affect runtime and JSON schema.
- Semantic presentation API that keeps valence and arousal internal.
- Unity runtime adapter and Unity Test Runner test sources.
- Unreal Engine runtime plugin with snapshot support and Blueprint-safe semantic output.
- Optional Unreal `AffectProfileComponent` and `AffectWorldSubsystem`.
- Actor Tag rules that turn newly-added, explicit gameplay tags into bounded affect updates.
- TypeScript CI workflow and public MIT license.

### Fixed

- Events no longer reset the decay clock; high-frequency events cannot indefinitely prevent natural recovery.
- Snapshot restoration validates every character before replacing runtime state.
- Presentation exposes a safe `recentCause` source category while raw cause IDs remain internal.
- Source caps, decay, thresholds, and dedupe window are configurable in the TypeScript reference runtime.
- Removed the obsolete Unreal verification disclaimer.

### Verified

- TypeScript core tests pass locally.
- Unreal Engine 5.7 plugin compiles and the `MyAiTown.AffectCore` automation group passes.

### Known boundary

- Unity tests are included but have not yet been executed in a locally installed Unity Editor.
