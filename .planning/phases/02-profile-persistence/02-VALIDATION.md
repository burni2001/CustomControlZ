---
phase: 2
slug: profile-persistence
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — Win32 binary, manual smoke tests only |
| **Config file** | None |
| **Quick run command** | Manual: launch app, verify profile loads and game select shows correct names |
| **Full suite command** | Manual: run all 4 Phase 2 success criteria scenarios |
| **Estimated runtime** | ~5 minutes (manual) |

---

## Sampling Rate

- **After every task commit:** Visually inspect the change (build succeeds, no compile errors)
- **After every plan wave:** Run full manual smoke tests for requirements covered by that wave
- **Before `/gsd:verify-work`:** All 4 success criteria must be confirmed manually
- **Max feedback latency:** ~5 minutes (manual smoke test run)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 2-01-01 | 01 | 0 | PROF-01, PROF-02, PROF-03, PROF-04 | manual-smoke | Create `profiles\` with EldenRing.json + ToxicCommando.json, verify files exist | ❌ W0 | ⬜ pending |
| 2-02-01 | 02 | 1 | PROF-01, PROF-02 | manual-smoke | Launch app, confirm 2 game names shown; remove one JSON, relaunch, confirm 1 game | ❌ W0 | ⬜ pending |
| 2-02-02 | 02 | 1 | PROF-02 | manual-smoke | Copy a third JSON to `profiles\`, relaunch, confirm 3 games appear | ❌ W0 | ⬜ pending |
| 2-03-01 | 03 | 2 | PROF-03 | manual-smoke | Select Elden Ring, verify sprint/dodge/longpress behaviors; select TC, verify scroll-wheel | ❌ W0 | ⬜ pending |
| 2-04-01 | 04 | 2 | PROF-04 | manual-smoke | Delete settings.ini, launch app, open settings — bindings show JSON `currentVk` values | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `profiles/EldenRing.json` — covers PROF-01, PROF-02, PROF-03, PROF-04 for Elden Ring profile
- [ ] `profiles/ToxicCommando.json` — covers PROF-01, PROF-02, PROF-03, PROF-04 for Toxic Commando profile
- [ ] `CustomControlZ/ProfileLoader.h` — new profile loading/saving module (stub acceptable in Wave 0)

*No automated test framework needed — manual smoke testing is the established validation approach for this project.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| `profiles\` folder discovery and per-game file mapping | PROF-01 | Win32 system-tray app; no automated test infra | (1) Create `profiles\` with 2 JSON files, launch — see 2 games. (2) Remove one JSON, relaunch — see 1 game. |
| Auto-discovery of new JSON files without code change | PROF-02 | Observable only via live app behavior | Copy a third JSON file to `profiles\`, relaunch — see 3 games in select window |
| Elden Ring and Toxic Commando behavior parity | PROF-03 | Behavior requires live input testing | Select Elden Ring, test sprint/dodge/longpress; select TC, verify scroll-wheel toggle |
| Settings.ini migration — bindings load from JSON | PROF-04 | Requires deleting settings.ini to simulate fresh state | Delete settings.ini, launch app, open settings — bindings show JSON `currentVk` defaults |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5 minutes
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
