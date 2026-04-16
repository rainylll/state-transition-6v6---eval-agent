# Phase 10.10 Report

## Title
Eligibility-aware blue-side first-kill fix

## Commands

```powershell
New-Item -ItemType Directory -Force -Path agent_mvp/data_world_model_cf/phase10_10_blue_eligible_fix | Out-Null
Copy-Item -Force agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt agent_mvp/data_world_model_cf/phase10_10_blue_eligible_fix/model.pt
C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phase10_10_blue_eligible_fix --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --stage2-red-firstkill-positive-repeat 6 --stage2-blue-firstkill-positive-repeat 10 --stage2-red-firstkill-hard-negative-repeat 3 --stage2-blue-firstkill-hard-negative-repeat 5 --device cuda --seed 7
C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_10_blue_eligible_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_10_blue_eligible_fix_strict_eval.json --splits heldout_cf --device cuda
C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_10_blue_eligible_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_10_blue_eligible_fix_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cuda
C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix_strict_eval_rerun_1010.json --splits heldout_cf --device cuda
```

## Eligibility

- Implemented in `train_world_model.py` as `annotate_first_kill_eligibility(records)`.
- For each `episode_id + t_index`, terminal-horizon `target_event` is used to infer whether any `first_kill` still exists in the future.
- Added meta fields:
  - `prior_kill_seen`
  - `red_first_kill_eligible`
  - `blue_first_kill_eligible`
  - `future_red_first_kill_possible`
  - `future_blue_first_kill_possible`

## Stage2 Sampling Summary

- `base_count = 160`
- `expanded_count = 488`
- `red_first_kill_positive_count = 28`
- `blue_first_kill_positive_count = 12`
- `red_first_kill_hard_negative_count = 40`
- `blue_first_kill_hard_negative_count = 0`
- `blue_eligible_count = 80`
- `blue_ineligible_filtered_count = 40`

## Strict Eval: 10.8 vs 10.10

- `event_first_kill_accuracy`: `0.7000 -> 0.66875`
- `red_first_kill_accuracy`: `0.6875 -> 0.6875`
- `blue_first_kill_accuracy`: `0.7125 -> 0.6500`
- `blue_first_kill_precision`: `0.2333 -> 0.2000`
- `blue_first_kill_pred_positive_rate`: `0.3750 -> 0.4375`
- `blue_first_kill_false_positive_rate`: `0.3151 -> 0.3836`
- `blue_first_kill_eligible_subset_accuracy`: `0.7273 -> 0.7045`
- `blue_first_kill_eligible_subset_precision`: `0.3684 -> 0.3500`
- `critical_event_accuracy`: `0.7650 -> 0.7525`
- `termination_flag_accuracy`: `0.9625 -> 0.9625`
- `termination_flag_high_conf_hit_rate`: `1.0000 -> 1.0000`
- `reward_red_mae`: `3.1468 -> 3.1356`
- `reward_total_mae`: `3.0744 -> 3.1699`
- `terminal_win_accuracy`: `0.5375 -> 0.5375`
- `trajectory_alive_accuracy`: `0.9083 -> 0.9083`

## Held-out Counterfactual: 10.8 vs 10.10

- `event_blue_first_kill_prob sign_acc`: `0.4 -> 0.5`
- `event_blue_first_kill_prob delta_mae`: `0.1252025 -> 0.1251488`
- `termination_flag sign_acc`: `0.4167 -> 0.1667`
- `reward_red sign_acc`: `0.6102 -> 0.6102`
- `terminal_red_win_prob sign_acc`: `0.6705 -> 0.6705`

## Decision

- Eligibility-aware filtering **kept** the blue-side counterfactual sign improvement at `0.5`.
- But it **did not restore strict calibration** to the 10.8 level.
- This 10.10 variant is therefore **NO-GO** as a replacement for 10.8.
