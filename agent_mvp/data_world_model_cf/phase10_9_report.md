# Phase 10.9 Blue First-Kill Targeted Fix Report

## Commands
- prepare_output_dir: `New-Item -ItemType Directory -Force -Path agent_mvp/data_world_model_cf/phase10_9_blue_firstkill_fix | Out-Null`
- prepare_checkpoint_copy: `Copy-Item -Force agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt agent_mvp/data_world_model_cf/phase10_9_blue_firstkill_fix/model.pt`
- train_10_9: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phase10_9_blue_firstkill_fix --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --stage2-red-firstkill-positive-repeat 6 --stage2-blue-firstkill-positive-repeat 10 --stage2-red-firstkill-hard-negative-repeat 3 --stage2-blue-firstkill-hard-negative-repeat 5 --device cuda --seed 7`
- strict_eval_10_9: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_9_blue_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_9_blue_firstkill_fix_strict_eval.json --splits heldout_cf --device cuda`
- counterfactual_eval_10_9: `C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_9_blue_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phase10_9_blue_firstkill_fix_counterfactual_eval.json --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --device cuda`

## Strict Eval (heldout_cf)
- event_first_kill_accuracy: 10.8=0.7 | 10.9=0.66875
- red_first_kill_accuracy: 10.8=0.6875 | 10.9=0.6875
- blue_first_kill_accuracy: 10.8=0.7125 | 10.9=0.65
- red_first_kill_positive_recall: 10.8=1.0 | 10.9=1.0
- blue_first_kill_positive_recall: 10.8=1.0 | 10.9=1.0
- red_first_kill_precision: 10.8=0.375 | 10.9=0.375
- blue_first_kill_precision: 10.8=0.23333333333333334 | 10.9=0.2
- red_first_kill_high_conf_hit_rate: 10.8=1.0 | 10.9=1.0
- blue_first_kill_high_conf_hit_rate: 10.8=1.0 | 10.9=1.0
- critical_event_accuracy: 10.8=0.7650000086054206 | 10.9=0.7525000080466271
- termination_flag_accuracy: 10.8=0.9625 | 10.9=0.9625
- termination_flag_high_conf_hit_rate: 10.8=1.0 | 10.9=1.0
- reward_red_mae: 10.8=3.1467823445796967 | 10.9=3.0741729974746703
- reward_total_mae: 10.8=3.074378564953804 | 10.9=3.1345853328704836
- terminal_win_accuracy: 10.8=0.5375 | 10.9=0.55
- trajectory_alive_accuracy: 10.8=0.9083333333333332 | 10.9=0.9083333333333332

## Held-out Counterfactual
- event_red_first_kill_prob: delta_mae 0.2750183632611879 -> 0.2749922829141724; signal_pairs 44 -> 44; sign_acc 0.6363636363636364 -> 0.6363636363636364
- event_blue_first_kill_prob: delta_mae 0.1252025414013133 -> 0.12514283079351002; signal_pairs 20 -> 20; sign_acc 0.4 -> 0.5
- termination_flag: delta_mae 0.07500086683007226 -> 0.07500223115370944; signal_pairs 12 -> 12; sign_acc 0.4166666666666667 -> 0.16666666666666666
- reward_red: delta_mae 0.45898618185892703 -> 0.4590376318432391; signal_pairs 59 -> 59; sign_acc 0.6101694915254238 -> 0.6101694915254238
- terminal_red_win_prob: delta_mae 0.5500929433852434 -> 0.550070534273982; signal_pairs 88 -> 88; sign_acc 0.6704545454545454 -> 0.6704545454545454

## Decision
- go_no_go: NO-GO to replace 10.8 with this 10.9 variant