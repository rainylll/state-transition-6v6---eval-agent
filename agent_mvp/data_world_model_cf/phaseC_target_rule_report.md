# Phase C Terminal Target Rule Rewrite Report

## Contract
- target_contract: `phaseC_v1`
- terminal-only rewrite of blue first-kill supervision boundary
- processed records now include `target_event_contract.blue_first_kill_flag`, `target_event_contract.blue_first_kill_supervision_mask`, `target_event_contract.terminal_critical_role`
- terminal_confusion_primary role_counts: {'blue_objective_only': 15, 'red_first_kill': 15}
- terminal_confusion_primary mask_counts: {0: 30}

## Heldout Pack (A vs C)
- event_first_kill_accuracy: A=0.7 | C=0.66875
- critical_event_accuracy: A=0.7650000086054206 | C=0.7525000080466271
- blue_first_kill_accuracy: A=0.7125 | C=0.65
- blue_first_kill_precision: A=0.23333333333333334 | C=0.2
- termination_flag_accuracy: A=0.9625 | C=0.9625
- reward_red_mae: A=3.1467823445796967 | C=3.1211633741855622
- reward_total_mae: A=3.074378564953804 | C=3.0647358536720275

## Terminal Confusion Primary Pack
### terminal_red_first_kill_outcome_confusion
- num_samples: A=15 | C=15
- blue_first_kill_pred_positive_rate: A=0.8 | C=0.8666666666666667
- blue_first_kill_false_positive_rate: A=0.8 | C=0.8666666666666667
- mean_pred_event_blue_first_kill_prob: A=0.7268585503101349 | C=0.7586602727572124

### terminal_blue_objective_without_first_kill
- num_samples: A=15 | C=15
- blue_first_kill_pred_positive_rate: A=0.6 | C=0.8
- blue_first_kill_false_positive_rate: A=0.6 | C=0.8
- mean_pred_event_blue_first_kill_prob: A=0.6225520074367523 | C=0.6680120865503947

## Counterfactual (selected)
### event_blue_first_kill_prob
- sign_acc: A=0.4 | C=0.5
- delta_mae: A=0.1252025414013133 | C=0.12521034015114196

### termination_flag
- sign_acc: A=0.4166666666666667 | C=0.25
- delta_mae: A=0.07500086683007226 | C=0.07500067473831393

### reward_red
- sign_acc: A=0.6101694915254238 | C=0.6101694915254238
- delta_mae: A=0.45898618185892703 | C=0.45896977530792354

### terminal_red_win_prob
- sign_acc: A=0.6704545454545454 | C=0.6704545454545454
- delta_mae: A=0.5500929433852434 | C=0.5500908009707928

## Commands
- train_C: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseC_terminal_target_rule_v1 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_6_ft_nocf/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --target-contract phaseC_v1 --device cuda --seed 7`
- strict_eval_A_heldout: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseB_A_10_8_heldout_strict.json --splits heldout_cf --device cuda`
- strict_eval_C_heldout: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phaseC_terminal_target_rule_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseC_terminal_target_rule_v1_heldout_strict.json --splits heldout_cf --device cuda`
- terminal_confusion_eval_A: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseB_A_10_8_terminal_confusion.json --splits terminal_confusion_primary_cf --device cuda`
- terminal_confusion_eval_C: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phaseC_terminal_target_rule_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseC_terminal_target_rule_v1_terminal_confusion.json --splits terminal_confusion_primary_cf --device cuda`
- counterfactual_eval_A: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cuda`
- counterfactual_eval_C: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phaseC_terminal_target_rule_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseC_terminal_target_rule_v1_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cuda`