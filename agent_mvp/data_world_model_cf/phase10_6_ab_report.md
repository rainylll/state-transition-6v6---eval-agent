# Phase10.5 vs Phase10.6 A/B Report

## Strict Eval (heldout_cf)
- trajectory_alive_accuracy: phase10.5=0.9083333333333332 | phase10.6=0.9083333333333332
- terminal_win_accuracy: phase10.5=0.525 | phase10.6=0.55
- critical_event_accuracy: phase10.5=0.7650000086054206 | phase10.6=0.7525000080466271
- termination_flag_accuracy: phase10.5=0.9625 | phase10.6=0.9625
- termination_flag_high_conf_hit_rate: phase10.5=1.0 | phase10.6=1.0
- event_first_fire_accuracy: phase10.5=0.85625 | phase10.6=0.85625
- event_fire_count_mae: phase10.5=0.7838523497863207 | phase10.6=0.8076997295138426
- reward_red_mae: phase10.5=2.9964044868946074 | phase10.6=2.9904629051685334
- reward_total_mae: phase10.5=3.014285999536514 | phase10.6=2.990074399113655

## Held-out Counterfactual Eval (heldout_cf)
- valid: phase10.5=True | phase10.6=True
- groups/pairs: phase10.5=16/160 | phase10.6=16/160
- event_red_fire_count: delta_mae 1.0253706853138282 -> 1.0253425266360865; signal_pairs 61 -> 61; sign_acc 0.5573770491803278 -> 0.5573770491803278
- termination_flag: delta_mae 0.07500016808894543 -> 0.07500017833523884; signal_pairs 12 -> 12; sign_acc 0.16666666666666666 -> 0.25
- reward_red: delta_mae 0.4590221271850169 -> 0.4589945667423308; signal_pairs 59 -> 59; sign_acc 0.6101694915254238 -> 0.6101694915254238
- terminal_red_win_prob: delta_mae 0.5501008929684759 -> 0.5500992350280285; signal_pairs 88 -> 88; sign_acc 0.6818181818181818 -> 0.6818181818181818