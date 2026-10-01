# Beta release checklist

- [x] Public source contains no detected secrets.
- [x] MIT license and changelog are present.
- [x] TypeScript core has local automated-test evidence.
- [x] Unreal Engine 5.7 has build and automation-test evidence.
- [ ] Run `MyAiTown.AffectCore.Unity.Tests` in the target Unity LTS version and attach its result to the release notes. Current reason: this verification machine has no Unity Editor installed, so no real Unity Test Runner result can be claimed.
- [ ] Review the staged Git diff and commit only `packages/affect-core` and `.github/workflows/affect-core.yml`.
- [ ] Create a GitHub release named `v0.1.0-beta.2` and paste the matching changelog section.
