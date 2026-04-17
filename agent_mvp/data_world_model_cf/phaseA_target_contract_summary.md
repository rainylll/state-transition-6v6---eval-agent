# Phase A Target Contract Summary

## Frozen Baseline
- baseline: `phase10_8_firstkill_fix`
- decision: keep 10.8 as the current main version; do not patch 10.9/10.10/10.11 further before target-definition cleanup

## Contract Decisions
- `first_kill` stays a future-window event target, not a success proxy.
- eligibility is auxiliary metadata for filtering/masking; it must not silently relabel objective or termination as `first_kill`.
- terminal horizon negative rules:
  - blue objective success without blue first kill must remain blue_first_kill=0
  - red first-kill dominated terminal outcomes must remain blue_first_kill=0
  - no-first-kill-remaining terminal endings must keep both first_kill flags at 0

## Terminal Confusion Pack
- pack_dir: `D:\sky\Projects\state-transition 6v6 + eval agent\agent_mvp\data_world_model_cf\terminal_confusion_pack`
- primary_records: `30`
- extended_records: `33`
- primary bucket `terminal_blue_objective_without_first_kill`: `15`
- primary bucket `terminal_red_first_kill_outcome_confusion`: `15`

## How To Consume The Pack
- use `terminal_confusion_primary_cf.jsonl` as the main terminal side-confusion regression set
- use `terminal_confusion_extended_cf.jsonl` when also tracking no-first-kill-remaining terminal endings
- keep this pack eval-only; do not mix it into training directly

## Recommended Next Step
- preferred: **new target definition comparison**
- not preferred: more repeat / weight / budget patching on the current contract

## Suggested A/B/C
- A: freeze 10.8 contract and report baseline on heldout_pack + terminal_confusion_primary_cf
- B: introduce side-aware terminal qualification / masking only for blue first_kill target consumption, no backbone changes
- C: if B shows clean strict gain on terminal_confusion pack, add minimal training-side use of the new qualification while keeping 10.8 reward/termination settings fixed