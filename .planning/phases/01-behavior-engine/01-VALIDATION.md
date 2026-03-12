---
phase: 1
slug: behavior-engine
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-12
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | None — Win32 C++ binary with no existing test infrastructure |
| **Config file** | none — no automated framework |
| **Quick run command** | Manual: launch app, activate profile, verify behavior |
| **Full suite command** | Manual: run all 5 success criteria scenarios (~5 min) |
| **Estimated runtime** | ~5 minutes (manual) |

---

## Sampling Rate

- **After every task commit:** Manual smoke test for the specific behavior touched
- **After every plan wave:** Run full manual suite against all 5 success criteria
- **Before `/gsd:verify-work`:** Full manual suite must pass all criteria
- **Max feedback latency:** ~5 minutes (manual verification at wave boundaries)

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| BEH-01 | TBD | 1 | BEH-01 | manual-smoke | Hold sprint key in Elden Ring profile; confirm outputVk held while input held, released when input released | N/A | ⬜ pending |
| BEH-02 | TBD | 1 | BEH-02 | manual-smoke | Hold dodge key in Elden Ring profile; confirm single pulse fired, no repeat while held | N/A | ⬜ pending |
| BEH-03 | TBD | 1 | BEH-03 | manual-smoke | Tap trigger key (< threshold): confirm shortOutputVk fires; hold trigger key (≥ threshold): confirm longOutputVk fires | N/A | ⬜ pending |
| BEH-04 | TBD | 1 | BEH-04 | manual-smoke | Press scroll-up key in TC profile; confirm mouse wheel up event sent (weapon cycles in-game) | N/A | ⬜ pending |
| FIX-01 | TBD | 1 | FIX-01 | manual-smoke | Hold sprint in Elden Ring mode, click Exit button on tray menu; confirm key not stuck after app closes | N/A | ⬜ pending |
| FIX-02 | TBD | 1 | FIX-02 | manual-smoke | Launch target game as Administrator, activate profile in app; confirm warning MessageBox is shown | N/A | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

None — Existing infrastructure covers all phase requirements.

No automated test infrastructure exists or is required for Phase 1. This is a Win32 system-tray application that injects input via `SendInput` into a live game process. All behaviors have observable, deterministic effects verifiable manually in under 5 minutes.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| HoldToToggle: hold input → output held; release → output released | BEH-01 | SendInput injects into OS input queue; requires live game process to observe effect | Run app, activate Elden Ring profile, hold sprint key, verify sprint activates; release, verify sprint stops |
| EdgeTrigger: press key → single pulse, no repeat while held | BEH-02 | Pulse timing requires human observation of single vs. repeated in-game action | Run app, Elden Ring profile, hold dodge key, confirm only one dodge fires |
| LongPress: tap → shortOutputVk; sustained hold → longOutputVk | BEH-03 | Two-threshold timing behavior requires live observation of two distinct outcomes | Tap trigger key quickly; confirm short action fires. Hold trigger key for >400ms; confirm long action fires |
| WheelToKey: key press → mouse wheel event | BEH-04 | Mouse wheel injection observed via game weapon cycle or OS scroll behavior | Run app, TC profile, press mapped key, confirm wheel-up event fires |
| RAII key release on graceful exit | FIX-01 | OS input queue state requires physical keyboard/game observation | Hold sprint, click Exit in tray; confirm sprint not stuck after process exits |
| UIPI warning shown when game is elevated | FIX-02 | Requires launching target process elevated; can only observe MessageBox at game-start transition | Launch game as admin, activate profile, confirm warning dialog appears |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 300s (5 min manual suite)
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
