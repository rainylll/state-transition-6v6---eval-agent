# Phase S.2 Gate Stabilization Audit

## Commands
- A_seed7_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_derived`
- A_seed11_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_seed11 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 11 --terminal-self-master-mode self_derived`
- A_seed19_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_seed19 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 19 --terminal-self-master-mode self_derived`
- S2_seed7_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS2_self_gate_stable_w50_seed7 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_derived --self-gate-stability-weight 50`
- S2_seed11_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS2_self_gate_stable_w50_seed11 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 11 --terminal-self-master-mode self_derived --self-gate-stability-weight 50`
- S2_seed19_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS2_self_gate_stable_w50_seed19 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 19 --terminal-self-master-mode self_derived --self-gate-stability-weight 50`
- heldout_eval_template: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path <model.pt> --out-path <heldout.json> --splits heldout_cf --device cpu --terminal-self-master-mode self_derived`
- terminal_confusion_eval_template: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path <model.pt> --out-path <terminal_confusion.json> --splits terminal_confusion_primary_cf --device cpu --terminal-self-master-mode self_derived`

## Training Target
- A: self/non-self terminal master interface + `self_derived` terminal first-kill projection. No extra gate stabilization loss.
- S2: same as A, plus a safe-band penalty on `P(self_first_kill_terminal)`:
  - true `self_first_kill_terminal`: penalize if `p < 0.65`
  - true non-self terminal: penalize if `p > 0.35`
  - loss weight: `50.0`

## Heldout Summary
| metric | A_seed7 | A_seed11 | A_seed19 | S2_seed7 | S2_seed11 | S2_seed19 |
| --- | --- | --- | --- | --- | --- | --- |
| event_first_kill_accuracy | 0.84375 | 0.8375 | 0.6375 | 0.8375 | 0.8375 | 0.74375 |
| critical_event_accuracy | 0.8225000068545342 | 0.8200000086799264 | 0.740000007674098 | 0.8200000070035458 | 0.8200000086799264 | 0.7825000071898103 |
| blue_first_kill_accuracy | 0.925 | 0.925 | 0.5875 | 0.925 | 0.925 | 0.7875 |
| blue_first_kill_precision | 0.6 | 0.6 | 0.175 | 0.6 | 0.6 | 0.25 |
| termination_flag_accuracy | 0.9625 | 0.9625 | 0.9625 | 0.9625 | 0.9625 | 0.9625 |
| reward_red_mae | 3.1896230280399323 | 3.1204550087451937 | 3.110897696018219 | 3.1777683079242705 | 3.1164815425872803 | 3.1007165610790253 |
| reward_total_mae | 3.1452891498804094 | 3.0700075387954713 | 3.0766232192516325 | 3.139099273085594 | 3.0619131177663803 | 3.0638736516237257 |

## Terminal Confusion Summary
| metric | A_seed7 | A_seed11 | A_seed19 | S2_seed7 | S2_seed11 | S2_seed19 |
| --- | --- | --- | --- | --- | --- | --- |
| blue_first_kill_false_positive_rate | 0.06666666666666667 | 0.06666666666666667 | 1.0 | 0.06666666666666667 | 0.06666666666666667 | 0.43333333333333335 |
| terminal_red_first_kill_outcome_confusion::pred_positive_rate | 0.13333333333333333 | 0.13333333333333333 | 1.0 | 0.13333333333333333 | 0.13333333333333333 | 0.4666666666666667 |
| terminal_red_first_kill_outcome_confusion::mean_predicted_blue_prob | 0.4879976590474447 | 0.4830338180065155 | 0.5467285513877869 | 0.48770562012990315 | 0.47649795810381573 | 0.5030696233113606 |
| terminal_blue_objective_without_first_kill::pred_positive_rate | 0.0 | 0.0 | 1.0 | 0.0 | 0.0 | 0.4 |
| terminal_blue_objective_without_first_kill::mean_predicted_blue_prob | 0.4796362062295278 | 0.47859806021054585 | 0.5393554886182149 | 0.479063481092453 | 0.4718953788280487 | 0.49566017985343935 |

## Gate Distribution Audit
- A terminal blue non-self mean `P(self_first_kill_terminal)`: `[0.483816929658254, 0.48081593910853065, 0.543042020003001]`
- S2 terminal blue non-self mean `P(self_first_kill_terminal)`: `[0.4833845516045888, 0.4741966724395752, 0.49936490058898925]`
- Drift range shrank from `0.06222608089447029` to `0.02516822814941405`.
- A heldout blue non-self mean range: `0.06226214856812451`
- S2 heldout blue non-self mean range: `0.025318392298438352`
- A heldout blue self mean range: `0.06627956032752991`
- S2 heldout blue self mean range: `0.03131031990051275`

## Readout
- The new penalty does not make role argmax cleaner. It makes the gate distribution less seed-sensitive.
- `seed19` is no longer an all-positive collapse. Terminal blue FPR drops from `1.0` to `0.43333333333333335`.
- `seed19` heldout guardrails also recover:
  - `event_first_kill_accuracy`: `0.6375 -> 0.74375`
  - `critical_event_accuracy`: `0.740000007674098 -> 0.7825000071898103`
  - `blue_first_kill_accuracy`: `0.5875 -> 0.7875`
  - `blue_first_kill_precision`: `0.175 -> 0.25`
- All three S2 seeds beat the 10.8 terminal blue FPR baseline of `0.7`.
- All three S2 seeds stay above the 10.8 heldout blue precision baseline of `0.23333333333333334`.

## Verdict
- S2 materially stabilizes the `self_derived` gate and rescues the catastrophic seed19 failure mode.
- It is still not fully hardened: seed19 remains too close to the `0.5` boundary, and the mechanism is still calibration-dominant rather than role-classification-dominant.
- This is strong enough to keep as the main route, but not yet strong enough to declare a fully solid new main version.
