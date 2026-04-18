# Phase S.3 Safe-Band Local Tuning

## Scope
- No backbone changes.
- No target redesign.
- No new heads.
- Only very local safe-band tuning around the existing `self_derived` gate.

## Experiments
- `S2_w50_6535`: baseline from S.2, weight `50`, margins `(0.65, 0.35)`
- `S3_w80_6535`: only increase weight to `80`
- `S3_w50_6030`: keep weight `50`, tighten margins to `(0.60, 0.30)`
- `S3_w80_6030`: combine weight `80` with margins `(0.60, 0.30)`

## Commands
- `S3_w80_6535` seed7: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_w80_seed7 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_derived --self-gate-stability-weight 80`
- `S3_w80_6535` seed11: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_w80_seed11 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 11 --terminal-self-master-mode self_derived --self-gate-stability-weight 80`
- `S3_w80_6535` seed19: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_w80_seed19 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 19 --terminal-self-master-mode self_derived --self-gate-stability-weight 80`
- `S3_w50_6030` seed7: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_m6030_seed7 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_derived --self-gate-stability-weight 50 --self-gate-positive-margin 0.60 --self-gate-negative-margin 0.30`
- `S3_w50_6030` seed11: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_m6030_seed11 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 11 --terminal-self-master-mode self_derived --self-gate-stability-weight 50 --self-gate-positive-margin 0.60 --self-gate-negative-margin 0.30`
- `S3_w50_6030` seed19: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_m6030_seed19 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 19 --terminal-self-master-mode self_derived --self-gate-stability-weight 50 --self-gate-positive-margin 0.60 --self-gate-negative-margin 0.30`
- `S3_w80_6030` seed7: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_w80_m6030_seed7 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_derived --self-gate-stability-weight 80 --self-gate-positive-margin 0.60 --self-gate-negative-margin 0.30`
- `S3_w80_6030` seed11: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_w80_m6030_seed11 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 11 --terminal-self-master-mode self_derived --self-gate-stability-weight 80 --self-gate-positive-margin 0.60 --self-gate-negative-margin 0.30`
- `S3_w80_6030` seed19: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS3_safeband_w80_m6030_seed19 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 19 --terminal-self-master-mode self_derived --self-gate-stability-weight 80 --self-gate-positive-margin 0.60 --self-gate-negative-margin 0.30`

## Key Result
- Tightening the negative side to `0.30` does push terminal blue non-self means farther below `0.5`, but it does so by over-suppressing blue first-kill everywhere.
- The best local tradeoff is still `S3_w80_6535`: same margins as S.2, only raise the weight.

## Best Tradeoff: `S3_w80_6535`
| seed | heldout event FK acc | heldout critical acc | heldout blue FK acc | heldout blue precision | terminal blue FPR | terminal blue non-self mean |
| --- | --- | --- | --- | --- | --- | --- |
| 7 | 0.83125 | 0.8175000071525573 | 0.925 | 0.6 | 0.06666666666666667 | 0.48238634566466015 |
| 11 | 0.83125 | 0.817500009573996 | 0.9125 | null | 0.0 | 0.4715121040741603 |
| 19 | 0.8125 | 0.8100000074133277 | 0.925 | 0.6 | 0.06666666666666667 | 0.4832107384999593 |

## Comparison To S.2
- S.2 terminal blue FPRs: `[0.0667, 0.0667, 0.4333]`
- `S3_w80_6535` terminal blue FPRs: `[0.0667, 0.0000, 0.0667]`
- S.2 terminal blue non-self mean drift: `0.0252`
- `S3_w80_6535` terminal blue non-self mean drift: `0.0117`

## Why The Other Two Are Too Aggressive
### `S3_w50_6030`
- terminal blue FPR becomes `[0.0, 0.0, 0.0]`
- but heldout blue pred-positive rate also becomes `[0.0, 0.0, 0.0]`
- heldout blue precision is undefined in all three seeds

### `S3_w80_6030`
- also drives terminal blue FPR to `[0.0, 0.0, 0.0]`
- but collapses heldout blue positives even harder
- seed19 heldout blue self mean falls to `0.4441`, which is already below the decision boundary

## Verdict
- There is a real local improvement available beyond S.2: raising only the safe-band weight to `80` is enough to unstick seed19 from the boundary and bring its terminal blue FPR down to `0.0667`.
- But the remaining risk is now clearer: push too hard on margins and the model stops emitting blue first-kill positives on heldout altogether.
- So the route is stronger than S.2, but not yet fully hardened as a new main version because `seed11` under `S3_w80_6535` already shows over-suppression on heldout blue positives.
