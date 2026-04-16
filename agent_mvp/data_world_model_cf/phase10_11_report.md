# Phase 10.11 Report

## Commands

- Train: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phase10_11_blue_terminal_filter_fix --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --stage2-blue-terminal-conflict-repeat 4 --device cuda --seed 7`
- Strict eval (10.11): `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_11_blue_terminal_filter_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_11_blue_terminal_filter_fix_strict_eval.json --splits heldout_cf --device cuda`
- Strict eval (10.8 rerun): `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix_strict_eval_rerun_1011.json --splits heldout_cf --device cuda`
- Counterfactual eval (10.11): `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_11_blue_terminal_filter_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_11_blue_terminal_filter_fix_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cuda`

## Stage2 Summary



## Stage2 Sampling Summary

- base_count: `160`
- expanded_count: `480`
- first_kill_positive_count: `40`
- first_kill_hard_negative_count: `40`
- positive_repeat: `6`
- hard_negative_repeat: `3`
- red_first_kill_positive_count: `28`
- blue_first_kill_positive_count: `12`
- red_first_kill_hard_negative_count: `40`
- blue_first_kill_hard_negative_count: `0`
- blue_terminal_conflict_count: `40`
- blue_eligible_count: `80`
- blue_ineligible_filtered_count: `40`
- red_positive_repeat: `6`
- blue_positive_repeat: `6`
- red_hard_negative_repeat: `3`
- blue_hard_negative_repeat: `3`
- blue_terminal_conflict_repeat: `4`
- mode: `first_kill_focus`

## Strict 10.8 vs 10.11

| metric | 10.8 | 10.11 |
|---|---:|---:|
| event_first_kill_accuracy | 0.700000 | 0.668750 |
| red_first_kill_accuracy | 0.687500 | 0.687500 |
| blue_first_kill_accuracy | 0.712500 | 0.650000 |
| blue_first_kill_precision | 0.233333 | 0.200000 |
| blue_first_kill_pred_positive_rate | 0.375000 | 0.437500 |
| blue_first_kill_false_positive_rate | 0.315068 | 0.383562 |
| blue_first_kill_terminal_false_positive_rate | 0.696970 | 0.848485 |
| critical_event_accuracy | 0.765000 | 0.752500 |
| termination_flag_accuracy | 0.962500 | 0.962500 |
| termination_flag_high_conf_hit_rate | 1.000000 | 1.000000 |
| reward_red_mae | 3.146782 | 3.117918 |
| reward_total_mae | 3.074379 | 3.088512 |
| terminal_win_accuracy | 0.537500 | 0.537500 |
| trajectory_alive_accuracy | 0.908333 | 0.908333 |
| terminal_red_outcome_confusion_rate | 0.800000 | 0.866667 |
| terminal_blue_objective_confusion_rate | 0.600000 | 0.800000 |

## Counterfactual 10.8 vs 10.11

### event_blue_first_kill_prob

| field | 10.8 | 10.11 |
|---|---:|---:|
| num_signal_pairs | 20 | 20 |
| delta_mae | 0.125203 | 0.125186 |
| mean_true_abs_delta | 0.125000 | 0.125000 |
| mean_pred_abs_delta | 0.000248 | 0.000231 |
| sign_accuracy_on_signal_pairs | 0.400000 | 0.500000 |

### termination_flag

| field | 10.8 | 10.11 |
|---|---:|---:|
| num_signal_pairs | 12 | 12 |
| delta_mae | 0.075001 | 0.075002 |
| mean_true_abs_delta | 0.075000 | 0.075000 |
| mean_pred_abs_delta | 0.000001 | 0.000003 |
| sign_accuracy_on_signal_pairs | 0.416667 | 0.416667 |

### reward_red

| field | 10.8 | 10.11 |
|---|---:|---:|
| num_signal_pairs | 59 | 59 |
| delta_mae | 0.458986 | 0.459026 |
| mean_true_abs_delta | 0.458576 | 0.458576 |
| mean_pred_abs_delta | 0.001210 | 0.001235 |
| sign_accuracy_on_signal_pairs | 0.610169 | 0.610169 |

### terminal_red_win_prob

| field | 10.8 | 10.11 |
|---|---:|---:|
| num_signal_pairs | 88 | 88 |
| delta_mae | 0.550093 | 0.550091 |
| mean_true_abs_delta | 0.550000 | 0.550000 |
| mean_pred_abs_delta | 0.000821 | 0.000813 |
| sign_accuracy_on_signal_pairs | 0.670455 | 0.670455 |
