# Phase R1 Mirror-Coupled Decision Report

## A. What Changed
- File: `agent_mvp/python/world_model.py`
- Scope: terminal `first_kill` derivation only
- Patch: new no-retrain decision mode `self_derived_mirror_coupled`
- Rule:
  - keep terminal blue `first_kill` derivation unchanged
  - clip terminal red `first_kill` derivation by the mirrored blue-view self gate
  - weights, trunk, target, safe-band config, and checkpoint stay frozen

## B. Why This Is Interface Repair, Not Retuning
- No retraining
- No hyperparameter sweep
- No backbone or target changes
- The patch only changes how terminal `red_first_kill` is exported from the already-frozen dual-view gate
- This directly tests whether P2 failed because the exported terminal decision was missing a mirror closure

## C. Did P1 Main Surface Hold
- Yes
- Heldout, all three seeds:
  - `event_first_kill_accuracy = 0.86875`
  - `critical_event_accuracy = 0.83250`
  - `blue_first_kill_accuracy = 0.925`
  - `blue_first_kill_precision = 0.6`
  - `blue_first_kill_pred_positive_rate = 0.0625`
  - `termination_flag_accuracy = 0.9625`
- Terminal confusion, all three seeds:
  - `blue_first_kill_false_positive_rate = 0.0667`
  - `terminal_red_first_kill_outcome_confusion pred_positive_rate = 0.1333`
  - `terminal_blue_objective_without_first_kill pred_positive_rate = 0.0000`
- No seed fell back to `pred_positive_rate = 0.0`
- No seed returned `precision = null`

## D. Did P2 Mirror Imbalance Improve
- Yes at the patched decision layer
- Heldout post-decision red/blue non-self gap:
  - seed7: `0.00039`
  - seed11: `0.00211`
  - seed19: `0.00209`
- Terminal-confusion post-decision red/blue non-self gap:
  - seed7: `0.00434`
  - seed11: `0.00234`
  - seed19: `0.00368`
- Terminal-confusion red control `pred_positive_rate`:
  - seed7: `0.0`
  - seed11: `0.0`
  - seed19: `0.0`
- Blue control stayed low:
  - heldout: `0.0606`
  - terminal_confusion: `0.0667`

Important note:
- The upstream `terminal_self_role` gate itself is unchanged and still asymmetric
- R1 only repaired the exported terminal `first_kill` decision interface
- So the improvement is real, but it lives in the mirror-coupled decision layer, not in the frozen self-role classifier body

## E. Final Verdict
- `R1 passed`
- Decision: `restore promotion`
- promotion resumed for the R1 mirror-coupled decision-interface candidate; upstream gate asymmetry remains a documented caveat.
- Interpretation:
  - P2 was not purely a backbone / training maturity failure
  - A clean no-retrain mirror closure at the terminal decision layer is enough to preserve P1 and remove the observed mirror mismatch on the exported `first_kill` interface
- Remaining caution:
  - The frozen upstream self-role gate is still mirror-asymmetric
  - Promotion should therefore resume on the patched decision interface, with that limitation recorded explicitly
