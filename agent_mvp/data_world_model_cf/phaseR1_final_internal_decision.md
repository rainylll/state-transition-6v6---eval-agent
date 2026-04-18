# Phase R1 Final Internal Decision

## Decision
- `Conditional Go`

## Official Status Line
- `Promotion resumed for the R1 mirror-coupled decision-interface candidate; upstream terminal_self_role gate asymmetry remains a documented caveat.`
- `当前恢复晋升的对象仅限 R1 镜像耦合决策接口候选版；上游 terminal_self_role gate 仍存在未完全对称的已记录风险。`

## Formal Main Version
- `10.8` remains the formal stable main version.

## Current Candidate
- Official candidate name: `phaseR1_candidate_s4_1_110_mirror_coupled`

## What R1 Actually Fixed
- R1 repaired the exported terminal `first_kill` decision interface.
- It did so with a no-retrain mirror-coupled derivation / decision patch.
- The patched decision layer kept the blue-facing P1 surface intact while shrinking red-blue post-decision non-self gaps to the `0.002 ~ 0.004` range and reducing terminal red control `pred_positive_rate` to `0.0`.

## What R1 Did Not Fix
- R1 did not make the upstream `terminal_self_role` gate itself fully mirror-symmetric.
- That asymmetry remains visible in the upstream self-gate diagnostics and must stay documented.

## Internal Audit Checks
- P1 main surface still holds:
  - all three seeds keep `blue_first_kill_pred_positive_rate > 0`
  - heldout `blue_first_kill_precision = 0.6` for all three seeds
  - blue-facing terminal `blue_first_kill_false_positive_rate = 0.0667` for all three seeds
  - heldout core metrics remain clearly above `10.8`
- P2 exported mirror issue is fixed at the decision layer:
  - heldout post-decision non-self gap max `= 0.00211`
  - terminal-confusion post-decision non-self gap max `= 0.00434`
  - terminal red control `pred_positive_rate = 0.0` for all three seeds
- Caveat is preserved:
  - no file here claims the upstream gate is already fully symmetric

## Final Interpretation
- The current repository state supports a `Conditional Go`.
- Promotion is resumed for the R1 mirror-coupled decision-interface candidate only.
- This is not a claim that the upstream gate is already a fully mature mirror-clean candidate.
