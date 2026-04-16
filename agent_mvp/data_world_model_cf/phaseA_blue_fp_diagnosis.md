# Phase A Blue False-Positive Diagnosis

## Scope
- Primary model: `phase10_8`
- Split: `heldout_cf`
- Records analyzed: `80`
- Blue false positives: `23`

## Key Finding
10.8 blue false positives are dominated by terminal_red_first_kill_outcome_confusion, terminal_blue_objective_without_first_kill, terminal_or_late_window_without_any_first_kill_remaining

## Primary Buckets
- `terminal_red_first_kill_outcome_confusion`: 12 cases (52.2%)
- `terminal_blue_objective_without_first_kill`: 9 cases (39.1%)
- `terminal_or_late_window_without_any_first_kill_remaining`: 2 cases (8.7%)

## Overlap Signals
- `prior_kill_seen`: 0 cases (0.0%)
- `future_red_first_kill_possible`: 12 cases (52.2%)
- `future_blue_first_kill_possible`: 0 cases (0.0%)
- `termination_overlap`: 21 cases (91.3%)
- `objective_overlap`: 21 cases (91.3%)
- `kill_delta_overlap`: 12 cases (52.2%)

## Horizon / Near-Terminal
- `terminal_horizon`: 23 cases (100.0%)

## Why 10.9 / 10.10 Can Improve Sign But Hurt Strict
- The dominant blue false-positive buckets are windows where blue first-kill should not be active yet or is no longer eligible, so pushing blue logits upward can improve pairwise direction on some counterfactual comparisons while still crossing the 0.5 strict threshold too often.
- If 10.9/10.10 raise blue first-kill probability in red-future or lost-eligibility windows, counterfactual sign can improve without improving strict precision.
- Because held-out strict uses only state_step=0 groups, t_index is degenerate here; the real failure mode is not temporal spread inside the episode but misclassification of horizon/eligibility semantics from the same initial state.

## Bucket Diagnostics
### terminal_red_first_kill_outcome_confusion
- Count: 12 (52.2% of blue false positives)
- Interpretation: Terminal windows where red owns the decisive kill/objective chain, but blue first-kill is still predicted.
- Model comparison:
  - `phase10_8`: mean_prob=0.8142, pred_positive_rate=1.000, false_positive_rate=1.000
  - `phase10_9`: mean_prob=0.8549, pred_positive_rate=1.000, false_positive_rate=1.000
  - `phase10_10`: mean_prob=0.8576, pred_positive_rate=1.000, false_positive_rate=1.000
- Example cases:
  - `sim_task_00036|terminal|0|5566` | horizon=terminal | prob=0.8759 | future_red=True | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=1.0 | objective_blue=0.0
  - `sim_task_00038|terminal|0|4465` | horizon=terminal | prob=0.8758 | future_red=True | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=1.0 | objective_blue=0.0
  - `sim_task_00089|terminal|0|3502` | horizon=terminal | prob=0.8498 | future_red=True | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=1.0 | objective_blue=0.0
  - `sim_task_00088|terminal|0|3610` | horizon=terminal | prob=0.8488 | future_red=True | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=1.0 | objective_blue=0.0
  - `sim_task_00066|terminal|0|6262` | horizon=terminal | prob=0.8486 | future_red=True | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=1.0 | objective_blue=0.0

### terminal_blue_objective_without_first_kill
- Count: 9 (39.1% of blue false positives)
- Interpretation: Terminal windows where blue objective completes, but there is still no blue first-kill; the model appears to conflate blue success with blue first-kill.
- Model comparison:
  - `phase10_8`: mean_prob=0.7718, pred_positive_rate=1.000, false_positive_rate=1.000
  - `phase10_9`: mean_prob=0.8314, pred_positive_rate=1.000, false_positive_rate=1.000
  - `phase10_10`: mean_prob=0.8340, pred_positive_rate=1.000, false_positive_rate=1.000
- Example cases:
  - `sim_task_00067|terminal|0|5680` | horizon=terminal | prob=0.8496 | future_red=False | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=0.0 | objective_blue=1.0
  - `sim_task_00069|terminal|0|5553` | horizon=terminal | prob=0.8496 | future_red=False | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=0.0 | objective_blue=1.0
  - `sim_task_00070|terminal|0|6424` | horizon=terminal | prob=0.8496 | future_red=False | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=0.0 | objective_blue=1.0
  - `sim_task_00055|terminal|0|2987` | horizon=terminal | prob=0.8444 | future_red=False | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=0.0 | objective_blue=1.0
  - `sim_task_00054|terminal|0|3422` | horizon=terminal | prob=0.8444 | future_red=False | future_blue=False | prior_kill_seen=False | termination=1.0 | objective_red=0.0 | objective_blue=1.0

### terminal_or_late_window_without_any_first_kill_remaining
- Count: 2 (8.7% of blue false positives)
- Interpretation: No side can still produce a first-kill in the future of this window, so a blue first-kill prediction is a pure late-window eligibility error.
- Model comparison:
  - `phase10_8`: mean_prob=0.8478, pred_positive_rate=1.000, false_positive_rate=1.000
  - `phase10_9`: mean_prob=0.8732, pred_positive_rate=1.000, false_positive_rate=1.000
  - `phase10_10`: mean_prob=0.8760, pred_positive_rate=1.000, false_positive_rate=1.000
- Example cases:
  - `sim_task_00087|terminal|0|9000` | horizon=terminal | prob=0.8498 | future_red=False | future_blue=False | prior_kill_seen=False | termination=0.0 | objective_red=0.0 | objective_blue=0.0
  - `sim_task_00047|terminal|0|9000` | horizon=terminal | prob=0.8458 | future_red=False | future_blue=False | prior_kill_seen=False | termination=0.0 | objective_red=0.0 | objective_blue=0.0

## Reviewer Recommendation
- Next cut should target blue false-positive buckets with side-aware eligibility or false-positive filtering, not larger repeat or broader critical-group reweighting.