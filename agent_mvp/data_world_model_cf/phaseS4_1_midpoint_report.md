# Phase S.4.1 Midpoint Operating-Point Check

## Rule
- Try only `positive_scale = 1.10` first.
- Try `1.15` only if `1.10` fails the success standard.

## Baseline
- `A = S3_w80_6535`
- fixed settings:
  - `self_gate_stability_weight = 80`
  - `self_gate_positive_margin = 0.65`
  - `self_gate_negative_margin = 0.35`
  - `self_gate_negative_scale = 1.0`

## Step A: `positive_scale = 1.10`
- train template:
  - `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir <out_dir> --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed <seed> --terminal-self-master-mode self_derived --self-gate-stability-weight 80 --self-gate-positive-margin 0.65 --self-gate-negative-margin 0.35 --self-gate-positive-scale 1.10 --self-gate-negative-scale 1.0`
- heldout eval template:
  - `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path <model.pt> --out-path <heldout.json> --splits heldout_cf --device cpu --terminal-self-master-mode self_derived`
- terminal confusion eval template:
  - `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path <model.pt> --out-path <terminal_confusion.json> --splits terminal_confusion_primary_cf --device cpu --terminal-self-master-mode self_derived`

### `seed7`
- heldout:
  - `event_first_kill_accuracy = 0.83125`
  - `critical_event_accuracy = 0.8175000071525573`
  - `blue_first_kill_accuracy = 0.925`
  - `blue_first_kill_precision = 0.6`
  - `blue_first_kill_pred_positive_rate = 0.0625`
  - `termination_flag_accuracy = 0.9625`
- terminal confusion:
  - `blue_first_kill_false_positive_rate = 0.0667`
  - red-confusion bucket: `pred_positive_rate = 0.1333`, `mean blue prob = 0.4887`
  - blue-objective bucket: `pred_positive_rate = 0.0`, `mean blue prob = 0.4800`
- gate:
  - terminal blue non-self mean = `0.4844`
  - heldout blue self mean = `0.5006`

### `seed11`
- heldout:
  - `event_first_kill_accuracy = 0.8375`
  - `critical_event_accuracy = 0.8200000086799264`
  - `blue_first_kill_accuracy = 0.925`
  - `blue_first_kill_precision = 0.6`
  - `blue_first_kill_pred_positive_rate = 0.0625`
  - `termination_flag_accuracy = 0.9625`
- terminal confusion:
  - `blue_first_kill_false_positive_rate = 0.0667`
  - red-confusion bucket: `pred_positive_rate = 0.1333`, `mean blue prob = 0.4757`
  - blue-objective bucket: `pred_positive_rate = 0.0`, `mean blue prob = 0.4710`
- gate:
  - terminal blue non-self mean = `0.4733`
  - heldout blue self mean = `0.4868`

### `seed19`
- heldout:
  - `event_first_kill_accuracy = 0.8125`
  - `critical_event_accuracy = 0.8100000074133277`
  - `blue_first_kill_accuracy = 0.925`
  - `blue_first_kill_precision = 0.6`
  - `blue_first_kill_pred_positive_rate = 0.0625`
  - `termination_flag_accuracy = 0.9625`
- terminal confusion:
  - `blue_first_kill_false_positive_rate = 0.0667`
  - red-confusion bucket: `pred_positive_rate = 0.1333`, `mean blue prob = 0.4916`
  - blue-objective bucket: `pred_positive_rate = 0.0`, `mean blue prob = 0.4843`
- gate:
  - terminal blue non-self mean = `0.4879`
  - heldout blue self mean = `0.5078`

### Drift
- terminal blue non-self mean range: `0.0146`
- heldout blue self mean range: `0.0210`

## Step B: `positive_scale = 1.15`
- skipped
- reason: `1.10` already satisfied the midpoint success standard

## Verdict
- `1.10` rescues `seed11`:
  - no more `pred_positive_rate = 0.0`
  - no more `precision = null`
- `1.10` keeps `seed19` safe:
  - terminal blue FPR stays at `0.0667`, not `0.4333`
- all three seeds stay far below the `10.8` terminal blue FPR baseline of `0.7000`
- all three seeds keep terminal blue non-self mean below `0.49`

Conclusion: a usable midpoint operating point was found at `positive_scale = 1.10`. This is strong enough to move into main-version candidate promotion review.
