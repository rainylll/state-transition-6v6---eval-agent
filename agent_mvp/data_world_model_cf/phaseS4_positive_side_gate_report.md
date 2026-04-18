# Phase S.4 Positive-Side Protection Gate Audit

## What Changed
- Added a very local loss split in [train_world_model.py](/d:/sky/Projects/state-transition%206v6%20+%20eval%20agent/agent_mvp/python/train_world_model.py:178):
  - `self_gate_positive_scale`
  - `self_gate_negative_scale`
- This keeps the existing safe-band structure intact, but lets the positive side be adjusted without changing the negative side.

## Experiment Set
- `A = S3_w80_6535`
- `S4_ps075`: positive-side relief, `positive_scale=0.75`, `negative_scale=1.0`
- `S4_ps125`: positive-side protection, `positive_scale=1.25`, `negative_scale=1.0`

Negative-side settings were kept fixed:
- `self_gate_stability_weight=80`
- `self_gate_positive_margin=0.65`
- `self_gate_negative_margin=0.35`

## Commands
- `S4_ps075`: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir <out_dir> --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed <seed> --terminal-self-master-mode self_derived --self-gate-stability-weight 80 --self-gate-positive-margin 0.65 --self-gate-negative-margin 0.35 --self-gate-positive-scale 0.75 --self-gate-negative-scale 1.0`
- `S4_ps125`: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir <out_dir> --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed <seed> --terminal-self-master-mode self_derived --self-gate-stability-weight 80 --self-gate-positive-margin 0.65 --self-gate-negative-margin 0.35 --self-gate-positive-scale 1.25 --self-gate-negative-scale 1.0`
- heldout eval: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path <model.pt> --out-path <heldout.json> --splits heldout_cf --device cpu --terminal-self-master-mode self_derived`
- terminal confusion eval: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path <model.pt> --out-path <terminal_confusion.json> --splits terminal_confusion_primary_cf --device cpu --terminal-self-master-mode self_derived`

## Results
### A = `S3_w80_6535`
- `seed7`: heldout `blue_first_kill_pred_positive_rate=0.0625`, `precision=0.6`, terminal FPR `0.0667`
- `seed11`: heldout `blue_first_kill_pred_positive_rate=0.0`, `precision=null`, terminal FPR `0.0`
- `seed19`: heldout `blue_first_kill_pred_positive_rate=0.0625`, `precision=0.6`, terminal FPR `0.0667`

### `S4_ps075`
- `seed7`: unchanged positive firing, terminal FPR still `0.0667`
- `seed11`: still `pred_positive_rate=0.0`, still `precision=null`
- `seed19`: remains safe, terminal FPR `0.0667`
- Interpretation: mild positive-side relief does not rescue the dead positive side.

### `S4_ps125`
- `seed7`: stays healthy, heldout `pred_positive_rate=0.0625`, terminal FPR `0.0667`
- `seed11`: rescued, heldout `pred_positive_rate=0.0625`, `precision=0.6`, terminal FPR `0.0667`
- `seed19`: re-opens, heldout `pred_positive_rate=0.25`, `precision=0.25`, terminal FPR `0.4333`
- Interpretation: positive-side protection can revive seed11, but the first working setting also re-opens seed19.

## Gate Distribution
### A
- terminal blue non-self means: `[0.4824, 0.4715, 0.4832]`
- heldout blue self means: `[0.4986, 0.4850, 0.5030]`

### `S4_ps075`
- terminal blue non-self means: `[0.4762, 0.4661, 0.4692]`
- heldout blue self means: `[0.4923, 0.4794, 0.4886]`

### `S4_ps125`
- terminal blue non-self means: `[0.4870, 0.4758, 0.4941]`
- heldout blue self means: `[0.5033, 0.4893, 0.5142]`

## Readout
- Positive-side relief moves both self and non-self means down. It does not revive seed11.
- Positive-side protection is the first thing that actually revives seed11.
- But once seed11 revives, seed19 moves back toward the boundary and its terminal confusion re-opens.

## Verdict
- Phase S is still the first main route.
- But S.4 does not yet yield a promotion-safe candidate.
- The exact tension is now clear:
  - protect positives too little: `seed11` stays dead
  - protect positives enough to revive `seed11`: `seed19` re-opens
