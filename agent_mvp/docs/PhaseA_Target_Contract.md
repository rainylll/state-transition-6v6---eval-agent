# Phase A Target Contract

## Scope

- Frozen baseline: `phase10_8_firstkill_fix`
- This document defines what the world model is allowed to learn for:
  - `first_kill`
  - `objective_complete`
  - `termination`
  - terminal-horizon outcome cases which previously caused blue-side false positives
- This document does **not** change the backbone or training system by itself.
- This document exists to align:
  - ingest semantics
  - training targets
  - strict eval
  - held-out counterfactual eval
  - terminal-confusion regression packs

## 1. Core Contract

### 1.1 `first_kill` is a future-window event, not a success proxy

- `red_first_kill_flag` means:
  - within the prediction window represented by the current sample, red achieves the first kill event for that episode.
- `blue_first_kill_flag` means:
  - within the prediction window represented by the current sample, blue achieves the first kill event for that episode.
- `first_kill` is **not**:
  - a proxy for winning
  - a proxy for objective success
  - a proxy for termination
  - a proxy for “the blue side eventually did well”

### 1.2 Eligibility is metadata, not the target itself

- The supervised target remains a horizon-aware future event.
- `*_eligible` is auxiliary metadata used to decide:
  - whether a sample should contribute to a particular event loss
  - whether a strict false positive belongs to a true semantic-confusion bucket
- Eligibility must not silently redefine the label.

## 2. Horizon-Aware Rules

### 2.1 Non-terminal horizons (`1`, `5`, `10`, `20`)

- Keep current interpretation:
  - `side_first_kill_flag = 1` iff that side achieves first kill within the window.
- Do not inject outcome semantics from `objective_complete` or `termination`.
- Non-terminal windows should remain event-oriented, not outcome-oriented.

### 2.2 Terminal horizon

- Terminal horizon is where the current contract is most fragile.
- For `horizon = terminal`, `blue_first_kill_flag` is positive **only if**:
  - the terminal target window truly contains a blue first-kill event.
- Terminal horizon must **not** convert any of the following into `blue_first_kill = 1`:
  - blue objective success without blue first kill
  - red-dominant decisive outcome
  - generic terminal completion
  - safety-limit/no-kill endings

## 3. Relationship Between `first_kill`, `objective`, and `terminal outcome`

### 3.1 Objective success is not a proxy for first kill

- `blue_objective_complete_flag = 1` does **not** imply `blue_first_kill_flag = 1`.
- `red_objective_complete_flag = 1` does **not** imply `red_first_kill_flag = 1`.

### 3.2 Red first-kill dominated terminal outcomes

When terminal outcome is driven by red-side first kill or red-side decisive elimination:

- `red_first_kill_flag = 1`
- `blue_first_kill_flag = 0`
- this case belongs to a red-dominant terminal outcome bucket, not a blue success bucket

### 3.3 Blue objective without blue first kill

When blue reaches objective completion but does not own the episode’s first kill:

- `blue_objective_complete_flag = 1`
- `blue_first_kill_flag = 0`
- this is a valid blue objective outcome
- it must remain a negative sample for `blue_first_kill`

### 3.4 No first kill remaining

If no side can still produce a first-kill event for the target window:

- `red_first_kill_flag = 0`
- `blue_first_kill_flag = 0`
- even if terminal is true
- even if objective flags are false
- even if the ending is `safety_limit`

## 4. Terminal Confusion Buckets and Required Labels

### Bucket A: `terminal_red_first_kill_outcome_confusion`

Definition:

- terminal horizon
- true `red_first_kill_flag = 1`
- true `blue_first_kill_flag = 0`
- often overlaps with:
  - `termination_flag = 1`
  - red objective / red decisive outcome

Required contract:

- train/eval must treat this as:
  - positive for red first kill
  - negative for blue first kill
- blue-side learning must not use this as a blue-positive proxy

### Bucket B: `terminal_blue_objective_without_first_kill`

Definition:

- terminal horizon
- true `blue_objective_complete_flag = 1`
- true `blue_first_kill_flag = 0`
- often overlaps with `termination_flag = 1`

Required contract:

- this remains a valid blue-objective case
- it remains a **negative** example for `blue_first_kill`
- later filtering/masking may downweight or isolate it for blue first-kill learning, but it must not be relabeled

### Bucket C: `terminal_or_late_window_without_any_first_kill_remaining`

Definition:

- terminal horizon
- no side can still produce first kill in the target window
- often appears with `safety_limit` or no-kill terminal endings

Required contract:

- both `red_first_kill` and `blue_first_kill` remain negative
- this is a pure late-window eligibility failure if predicted positive

## 5. Side-Aware Qualification Rules

The following rules are the minimum contract for future implementation.

### 5.1 `blue_first_kill_terminal_eligible`

At terminal horizon, a sample is eligible to train/evaluate blue first-kill semantics only if all are true:

1. the terminal target does not already express red-owned first kill as the decisive event
2. the terminal target is not merely blue objective success without blue first kill
3. the terminal target is not a no-first-kill-remaining ending

Equivalent negative conditions:

- `terminal_red_outcome_conflict = True`
- or `terminal_blue_objective_without_blue_first_kill_conflict = True`
- or `terminal_no_first_kill_remaining_conflict = True`

Then:

- `blue_first_kill_terminal_eligible = False`

### 5.2 `red_first_kill_terminal_eligible`

Symmetric logic should exist for red, but Phase A focuses on the blue-side terminal confusion because that is the proven bottleneck.

## 6. Positive and Negative Examples

### Positive example

- same-initial group, terminal horizon
- true `blue_first_kill_flag = 1`
- blue owns the first decisive kill in the terminal path
- objective/termination may also occur, but they are secondary consequences

### Negative example: blue objective without blue first kill

- `blue_objective_complete_flag = 1`
- `blue_first_kill_flag = 0`
- this is still blue success
- but must stay negative for the blue first-kill head

### Negative example: red first-kill dominates terminal outcome

- `red_first_kill_flag = 1`
- `blue_first_kill_flag = 0`
- terminal may be `blue_eliminated`
- this must never be converted into blue first-kill signal

### Negative example: no first kill remains

- terminal reached via `safety_limit` or another no-kill path
- both first-kill flags remain `0`

## 7. What Later Training/Eval Should Consume

### Training-side

- `first_kill` head still predicts a future-window event.
- Later training changes may use:
  - masking
  - filtering
  - downweighting
  - side-aware terminal qualification
- But no later patch should reinterpret objective/termination as first-kill labels.

### Strict eval

- Keep aggregate `critical_event_accuracy` only as summary.
- Primary diagnostics for this contract are:
  - `red_first_kill_accuracy`
  - `blue_first_kill_accuracy`
  - `blue_first_kill_precision`
  - `blue_first_kill_false_positive_rate`
  - `blue_first_kill_terminal_false_positive_rate`
  - `terminal_red_outcome_confusion_rate`
  - `terminal_blue_objective_confusion_rate`

### Counterfactual eval

- Counterfactual improvement is not sufficient by itself.
- Any future improvement must keep:
  - blue-side sign sensitivity
  - without lifting terminal-horizon false positives in the two dominant terminal confusion buckets

## 8. Explicit Anti-Rules

- Do not use `critical_event_accuracy` as a supervision shortcut.
- Do not treat `objective_complete` as a proxy label for `first_kill`.
- Do not treat `termination` as a proxy label for `first_kill`.
- Do not conclude that eligibility is useless based on `10.10`; that version used an overly coarse eligibility proxy.
- Do not introduce new training-side patches before target semantics are aligned with the contract above.

## 9. Phase A Deliverable Boundary

- `10.8` remains the frozen baseline.
- `10.9`, `10.10`, `10.11` remain diagnostic experiments only.
- The next implementation step should be a **new target-definition comparison**, not another repeat or group-weight sweep.
