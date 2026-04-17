# Phase D Terminal Role Head Report

## Commands
- prepare_output_dir: `New-Item -ItemType Directory -Force -Path agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1 | Out-Null`
- prepare_checkpoint_copy: `Copy-Item -Force agent_mvp/data_world_model_cf/phase10_6_ft_nocf/model.pt agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1/model.pt`
- train_D: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_6_ft_nocf/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --target-contract baseline --device cuda --seed 7`
- strict_eval_A: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseD_A_10_8_heldout_strict.json --splits heldout_cf --device cuda`
- strict_eval_D: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1_heldout_strict.json --splits heldout_cf --device cuda`
- terminal_confusion_eval_A: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseD_A_10_8_terminal_confusion.json --splits terminal_confusion_primary_cf --device cuda`
- terminal_confusion_eval_D: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1_terminal_confusion.json --splits terminal_confusion_primary_cf --device cuda`
- counterfactual_eval_A: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseD_A_10_8_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cuda`
- counterfactual_eval_D: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1/model.pt --out-path agent_mvp/data_world_model_cf/phaseD_terminal_role_head_v1_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cuda`

## Heldout A vs D
- event_first_kill_accuracy: A=0.7 | D=0.7
- critical_event_accuracy: A=0.7650000086054206 | D=0.7650000086054206
- blue_first_kill_accuracy: A=0.7125 | D=0.7125
- blue_first_kill_precision: A=0.23333333333333334 | D=0.23333333333333334
- termination_flag_accuracy: A=0.9625 | D=0.9625
- reward_red_mae: A=3.1467823445796967 | D=3.071038770675659
- reward_total_mae: A=3.074378564953804 | D=3.0270612686872482
- terminal_win_accuracy: A=0.5375 | D=0.525
- trajectory_alive_accuracy: A=0.9083333333333332 | D=0.9083333333333332

## Terminal Confusion A vs D
- blue_first_kill_false_positive_rate: A=0.7 | D=0.7
- critical_event_accuracy: A=0.560000017285347 | D=0.560000017285347
- blue_first_kill_accuracy: A=0.3 | D=0.3
- blue_first_kill_precision: A=0.0 | D=0.0
- terminal_red_first_kill_outcome_confusion:
  - pred_positive_rate: A=0.8 | D=0.8
  - false_positive_rate: A=0.8 | D=0.8
  - mean_pred_event_blue_first_kill_prob: A=0.7268585503101349 | D=0.7404542823632558
- terminal_blue_objective_without_first_kill:
  - pred_positive_rate: A=0.6 | D=0.6
  - false_positive_rate: A=0.6 | D=0.6
  - mean_pred_event_blue_first_kill_prob: A=0.6225520074367523 | D=0.6407701333363851

## Counterfactual A vs D
- event_blue_first_kill_prob: sign_acc A=0.4 | D=0.5; delta_mae A=0.1252025414013133 | D=0.1252173574858432
- termination_flag: sign_acc A=0.4166666666666667 | D=0.3333333333333333; delta_mae A=0.07500086683007226 | D=0.07500053940322289
- reward_red: sign_acc A=0.6101694915254238 | D=0.6101694915254238; delta_mae A=0.45898618185892703 | D=0.45899089993909004
- terminal_red_win_prob: sign_acc A=0.6704545454545454 | D=0.6931818181818182; delta_mae A=0.5500929433852434 | D=0.5500946585088968

## Terminal Role Head
- aggregate_terminal_role_accuracy: 0.5
- role_confusion_matrix:
  - blue_first_kill: {'blue_first_kill': 0, 'red_first_kill': 0, 'blue_objective_only': 0, 'no_first_kill_remaining': 0, 'other_terminal': 0}
  - red_first_kill: {'blue_first_kill': 0, 'red_first_kill': 15, 'blue_objective_only': 0, 'no_first_kill_remaining': 0, 'other_terminal': 0}
  - blue_objective_only: {'blue_first_kill': 0, 'red_first_kill': 15, 'blue_objective_only': 0, 'no_first_kill_remaining': 0, 'other_terminal': 0}
  - no_first_kill_remaining: {'blue_first_kill': 0, 'red_first_kill': 0, 'blue_objective_only': 0, 'no_first_kill_remaining': 0, 'other_terminal': 0}
  - other_terminal: {'blue_first_kill': 0, 'red_first_kill': 0, 'blue_objective_only': 0, 'no_first_kill_remaining': 0, 'other_terminal': 0}
- predicted_role_blue_prob_summary:
  - blue_first_kill: count=0, mean_blue_prob=0.0
  - red_first_kill: count=30, mean_blue_prob=0.6906122078498205
  - blue_objective_only: count=0, mean_blue_prob=0.0
  - no_first_kill_remaining: count=0, mean_blue_prob=0.0
  - other_terminal: count=0, mean_blue_prob=0.0
