# Phase P1 Promotion Harness

## Frozen Candidate
- candidate id: `phaseP1_candidate_s4_1_110`
- frozen config:
  - self/non-self terminal master interface
  - `self_derived` terminal first-kill projection
  - safe-band gate stabilization
  - `self_gate_stability_weight = 80`
  - `self_gate_positive_margin = 0.65`
  - `self_gate_negative_margin = 0.35`
  - `positive_scale = 1.10`
  - `negative_scale = 1.0`

No further tuning is included in this candidate.

## Fixed Inputs
- seeds: `7`, `11`, `19`
- eval packs:
  - `heldout_pack`
  - `terminal_confusion_primary_cf`
- baseline:
  - formal stable main version = `10.8`

## Results
### Seed 7
- heldout:
  - `event_first_kill_accuracy = 0.83125`
  - `critical_event_accuracy = 0.8175000071525573`
  - `blue_first_kill_accuracy = 0.925`
  - `blue_first_kill_precision = 0.6`
  - `blue_first_kill_pred_positive_rate = 0.0625`
  - `termination_flag_accuracy = 0.9625`
  - `reward_red_mae = 3.1643711507320402`
  - `reward_total_mae = 3.1363979667425155`
- terminal confusion:
  - `blue_first_kill_false_positive_rate = 0.0667`
  - `terminal_red_first_kill_outcome_confusion`: `pred_positive_rate = 0.1333`, `false_positive_rate = 0.1333`, `mean predicted blue prob = 0.4887`
  - `terminal_blue_objective_without_first_kill`: `pred_positive_rate = 0.0`, `false_positive_rate = 0.0`, `mean predicted blue prob = 0.4800`
- gate:
  - terminal blue non-self mean = `0.4844`
  - terminal blue self mean = `0.5006`

### Seed 11
- heldout:
  - `event_first_kill_accuracy = 0.8375`
  - `critical_event_accuracy = 0.8200000086799264`
  - `blue_first_kill_accuracy = 0.925`
  - `blue_first_kill_precision = 0.6`
  - `blue_first_kill_pred_positive_rate = 0.0625`
  - `termination_flag_accuracy = 0.9625`
  - `reward_red_mae = 3.1163239240646363`
  - `reward_total_mae = 3.061641752719879`
- terminal confusion:
  - `blue_first_kill_false_positive_rate = 0.0667`
  - `terminal_red_first_kill_outcome_confusion`: `pred_positive_rate = 0.1333`, `false_positive_rate = 0.1333`, `mean predicted blue prob = 0.4757`
  - `terminal_blue_objective_without_first_kill`: `pred_positive_rate = 0.0`, `false_positive_rate = 0.0`, `mean predicted blue prob = 0.4710`
- gate:
  - terminal blue non-self mean = `0.4733`
  - terminal blue self mean = `0.4868`

### Seed 19
- heldout:
  - `event_first_kill_accuracy = 0.8125`
  - `critical_event_accuracy = 0.8100000074133277`
  - `blue_first_kill_accuracy = 0.925`
  - `blue_first_kill_precision = 0.6`
  - `blue_first_kill_pred_positive_rate = 0.0625`
  - `termination_flag_accuracy = 0.9625`
  - `reward_red_mae = 3.1040005326271056`
  - `reward_total_mae = 3.0660919427871702`
- terminal confusion:
  - `blue_first_kill_false_positive_rate = 0.0667`
  - `terminal_red_first_kill_outcome_confusion`: `pred_positive_rate = 0.1333`, `false_positive_rate = 0.1333`, `mean predicted blue prob = 0.4916`
  - `terminal_blue_objective_without_first_kill`: `pred_positive_rate = 0.0`, `false_positive_rate = 0.0`, `mean predicted blue prob = 0.4843`
- gate:
  - terminal blue non-self mean = `0.4879`
  - terminal blue self mean = `0.5078`

## Gate Audit
- terminal blue non-self means: `[0.4844, 0.4733, 0.4879]`
- terminal blue non-self mean range: `0.0146`
- terminal blue self means: `[0.5006, 0.4868, 0.5078]`
- terminal blue self mean range: `0.0210`
- all three terminal blue non-self means remain below `0.49`

## P1 Checks
- no seed has `pred_positive_rate = 0.0`: passed
- no seed has `precision = null`: passed
- terminal `blue_first_kill_false_positive_rate <= 0.10`: passed
- heldout floor stays above the current `seed19` scale and above `10.8` guardrails: passed
- `10.8` core guardrails remain intact: passed

## Decision
**P1 passed: candidate can be promoted to new-version candidate status.**

### Recommendation
- keep `10.8` as formal stable main version
- promote `Phase S (S4.1 / 1.10)` to official new-version candidate status
- recommend entering `P2` for minimal mirror/generalization gate checks
