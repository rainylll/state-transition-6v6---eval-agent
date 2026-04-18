# Phase S Self/Non-Self Terminal Master Interface Report

## Commands
- A_reference_train_10_8: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phase10_8_firstkill_fix --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_6_ft_nocf/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode none`
- A_strict_eval: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseS_A_10_8_heldout_strict.json --splits heldout_cf --device cpu --terminal-self-master-mode none`
- A_terminal_confusion_eval: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseS_A_10_8_terminal_confusion.json --splits terminal_confusion_primary_cf --device cpu --terminal-self-master-mode none`
- A_counterfactual_eval: `python agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseS_A_10_8_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cpu --terminal-self-master-mode none`
- S_prepare_output_dir: `New-Item -ItemType Directory -Force -Path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1 | Out-Null`
- S_prepare_checkpoint_copy: `Copy-Item -Force agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1/model.pt`
- S_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_derived`
- S_strict_eval: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_heldout_strict.json --splits heldout_cf --device cpu --terminal-self-master-mode self_derived`
- S_terminal_confusion_eval: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_terminal_confusion.json --splits terminal_confusion_primary_cf --device cpu --terminal-self-master-mode self_derived`
- S_counterfactual_eval: `python agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cpu --terminal-self-master-mode self_derived`
- S_master_role_eval: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_terminal_confusion.json --splits terminal_confusion_primary_cf --device cpu --terminal-self-master-mode self_derived`

## Mapping Table
| source terminal semantic | red view | blue view |
| --- | --- | --- |
| red_first_kill | self_first_kill_terminal | non_self_terminal_critical |
| blue_first_kill | non_self_terminal_critical | self_first_kill_terminal |
| blue_objective_only | non_self_terminal_critical | non_self_terminal_critical |
| red_objective_only_or_decisive_non_fk | non_self_terminal_critical | non_self_terminal_critical |
| no_first_kill_remaining | other_terminal | other_terminal |
| other_terminal | other_terminal | other_terminal |

## Heldout Pack
| metric | A | S |
| --- | --- | --- |
| event_first_kill_accuracy | 0.7 | 0.84375 |
| critical_event_accuracy | 0.7650000086054206 | 0.8225000068545342 |
| blue_first_kill_accuracy | 0.7125 | 0.925 |
| blue_first_kill_precision | 0.23333333333333334 | 0.6 |
| termination_flag_accuracy | 0.9625 | 0.9625 |
| reward_red_mae | 3.1467823565006254 | 3.1896230280399323 |
| reward_total_mae | 3.074378550052643 | 3.1452891498804094 |

## Terminal Confusion
| metric | A | S |
| --- | --- | --- |
| blue_first_kill_false_positive_rate | 0.7 | 0.06666666666666667 |
| blue_first_kill_pred_positive_rate | 0.7 | 0.06666666666666667 |
| blue_first_kill_accuracy | 0.3 | 0.9333333333333333 |
| blue_first_kill_precision | 0.0 | 0.0 |
| event_first_kill_accuracy | 0.4 | 0.75 |
| critical_event_accuracy | 0.560000017285347 | 0.700000015894572 |

## Terminal Confusion Buckets
- terminal_red_first_kill_outcome_confusion:
  - A pred_positive_rate=0.8 | false_positive_rate=0.8 | mean_pred_blue_prob=0.7268585364023844
  - S pred_positive_rate=0.13333333333333333 | false_positive_rate=0.13333333333333333 | mean_pred_blue_prob=0.4879976590474447
- terminal_blue_objective_without_first_kill:
  - A pred_positive_rate=0.6 | false_positive_rate=0.6 | mean_pred_blue_prob=0.6225520153840383
  - S pred_positive_rate=0.0 | false_positive_rate=0.0 | mean_pred_blue_prob=0.4796362062295278

## Self-Role Diagnostics (S)
- heldout overall accuracy: 0.275
- heldout blue-view accuracy: 0.175
- terminal_confusion overall accuracy: 0.25
- terminal_confusion blue-view accuracy: 0.0
- heldout blue prob when true blue-view role != self_first_kill_terminal: 0.4837144839041161
- terminal_confusion blue prob when true blue-view role != self_first_kill_terminal: 0.48381693263848624