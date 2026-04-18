# Phase P2 Minimal Mirror / Generalization Gate Check

## Stress Test Choice
- test type: mirror consistency check
- method: compare red-view and blue-view gate suppression using the already frozen candidate outputs only
- no retraining
- no new hyperparameter changes

## Why This Test
- the candidate is explicitly built around a self/non-self symmetric interface
- the cheapest extra pressure test is to ask whether that symmetry actually holds in red-view as well as blue-view
- this directly tests whether the candidate is only good on the current blue-facing home pack

## Reconfirmed Main Surface
All three seeds still keep the P1 main-surface wins:
- heldout `blue_first_kill_pred_positive_rate > 0`
- heldout `blue_first_kill_precision` non-null
- terminal `blue_first_kill_false_positive_rate = 0.0667`
- heldout `event_first_kill_accuracy`, `critical_event_accuracy`, `blue_first_kill_accuracy`, `blue_first_kill_precision` all remain above `10.8`

## Mirror Results
### Seed 7
- heldout red non-self mean = `0.5003`
- heldout blue non-self mean = `0.4844`
- heldout red control pred-positive rate = `0.72`
- heldout blue control pred-positive rate = `0.0606`
- terminal red non-self mean = `0.5000`
- terminal blue non-self mean = `0.4844`
- terminal red control pred-positive rate = `0.8`
- terminal blue control pred-positive rate = `0.0667`

### Seed 11
- heldout red non-self mean = `0.4988`
- heldout blue non-self mean = `0.4737`
- heldout red control pred-positive rate = `0.4`
- heldout blue control pred-positive rate = `0.0606`
- terminal red non-self mean = `0.5097`
- terminal blue non-self mean = `0.4733`
- terminal red control pred-positive rate = `0.6`
- terminal blue control pred-positive rate = `0.0667`

### Seed 19
- heldout red non-self mean = `0.5201`
- heldout blue non-self mean = `0.4885`
- heldout red control pred-positive rate = `0.88`
- heldout blue control pred-positive rate = `0.0606`
- terminal red non-self mean = `0.5307`
- terminal blue non-self mean = `0.4879`
- terminal red control pred-positive rate = `1.0`
- terminal blue control pred-positive rate = `0.0667`

## Range Summary
- heldout red non-self mean range: `0.0213`
- heldout blue non-self mean range: `0.0148`
- terminal red non-self mean range: `0.0307`
- terminal blue non-self mean range: `0.0146`
- mean red-blue non-self gap on heldout: `0.0242`
- mean red-blue non-self gap on terminal confusion: `0.0316`

## Readout
- blue-facing gate remains stable and clearly below the `0.49` band
- red-view gate is not symmetric:
  - red non-self means are consistently higher than blue non-self means
  - in terminal confusion, red non-self means reach `0.5097` and `0.5307`
  - red control pred-positive rate is dramatically higher than blue control pred-positive rate
- this is a real mirror inconsistency, not just noise

## Decision
**P2 failed**

**candidate only survives on its home pack; keep 10.8 as formal stable main version and keep Phase S candidate unpromoted beyond current status.**

## Practical Positioning
- `10.8` remains the formal stable main version
- `Phase S (S4.1 / 1.10)` remains the strongest new-version candidate found so far
- but it does not yet pass the minimal mirror/generalization gate needed for stronger promotion beyond candidate status
