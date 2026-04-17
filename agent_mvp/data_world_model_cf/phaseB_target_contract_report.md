# Phase B Target Contract A/B Report

## Inputs and Rules
- A: current 10.8 contract / baseline targets
- B: phaseA_v1 contract consumption with terminal blue first-kill conflict weighting
- Shared training data: `agent_mvp/data_world_model_cf/ab_train_visible_only`
- Shared eval sets: `heldout_pack` and `terminal_confusion_primary_cf`

## Heldout Pack (A vs B)
- event_first_kill_accuracy: A=0.7 | B=0.7
- critical_event_accuracy: A=0.7650000086054206 | B=0.7650000086054206
- blue_first_kill_accuracy: A=0.7125 | B=0.7125
- blue_first_kill_precision: A=0.23333333333333334 | B=0.23333333333333334
- termination_flag_accuracy: A=0.9625 | B=0.9625
- reward_red_mae: A=3.1467823445796967 | B=3.055039381980896
- reward_total_mae: A=3.074378564953804 | B=3.0533773750066757

## Terminal Confusion Primary Pack
### terminal_red_first_kill_outcome_confusion
- num_samples: A=15 | B=15
- blue_first_kill_pred_positive_rate: A=0.8 | B=0.8
- blue_first_kill_false_positive_rate: A=0.8 | B=0.8
- mean_pred_event_blue_first_kill_prob: A=0.7268585503101349 | B=0.7446853677431743
- mean_true_event_blue_first_kill: A=0.0 | B=0.0

### terminal_blue_objective_without_first_kill
- num_samples: A=15 | B=15
- blue_first_kill_pred_positive_rate: A=0.6 | B=0.6
- blue_first_kill_false_positive_rate: A=0.6 | B=0.6
- mean_pred_event_blue_first_kill_prob: A=0.6225520074367523 | B=0.6471286892890931
- mean_true_event_blue_first_kill: A=0.0 | B=0.0

## Commands
- train_B: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseB_target_contract_v1 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --target-contract phaseA_v1 --device cuda --seed 7`
- strict_eval_A_heldout: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseB_A_10_8_heldout_strict.json --splits heldout_cf --device cuda`
- strict_eval_B_heldout: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phaseB_target_contract_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseB_B_contract_heldout_strict.json --splits heldout_cf --device cuda`
- strict_eval_A_terminal_confusion: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseB_A_10_8_terminal_confusion.json --splits terminal_confusion_primary_cf --device cuda`
- strict_eval_B_terminal_confusion: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phaseB_target_contract_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseB_B_contract_terminal_confusion.json --splits terminal_confusion_primary_cf --device cuda`