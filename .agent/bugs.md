# Known Problems And Boundaries

## Active Caveat
- The upstream `terminal_self_role` gate is still mirror-asymmetric.
- R1 repaired the exported terminal `first_kill` decision interface only.
- The current status is therefore `Conditional Go`, not “fully mature candidate”.

## What Is No Longer The Current Problem
- The exported terminal `first_kill` mirror mismatch that blocked `P2` is no longer the active blocker for the R1 candidate.
- Promotion is resumed for the R1 mirror-coupled decision-interface candidate.

## What Must Not Be Misstated
- Do not say the original pre-R1 candidate is fully mature.
- Do not say the upstream self-role gate has already become mirror-clean.
- Do not collapse "`R1 passed`" into "`all mirror asymmetry is solved`".

## Still Not Recommended
- Reopening `S4.x` local tuning
- Reopening positive / negative scale sweeps
- Returning to old exclude / repeat / weighting patch lines
- Starting a new model-change round just to make the caveat disappear

## Handoff Risk
- Future readers may look only at the strong P1 / blue-facing numbers and miss the caveat.
- The official status must therefore stay:
  - promotion resumed
  - for the R1 mirror-coupled decision-interface candidate
  - with upstream gate asymmetry as a documented caveat
