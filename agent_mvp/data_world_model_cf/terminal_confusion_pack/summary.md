# Terminal Confusion Pack Summary

## Scope
- baseline_model: phase10_8_firstkill_fix
- source_pack: agent_mvp\data_world_model_cf\heldout_pack
- source_split: heldout_cf
- intended_use: eval-only targeted pack for terminal side-confusion regressions

## Focus Buckets
- `terminal_blue_objective_without_first_kill`: 15
- `terminal_red_first_kill_outcome_confusion`: 15

## Coverage
- primary_records: 30
- primary_episodes: 30
- primary_tactic_combos: 5
- extended_records: 33
- extended_episodes: 33

## How To Use
- Use `terminal_confusion_primary_cf.jsonl` as the main regression set for terminal blue-side confusion.
- Use `terminal_confusion_extended_cf.jsonl` when you also want to track no-first-kill-remaining failures.
- Do not train on this pack directly; compare future target-definition variants against it.
