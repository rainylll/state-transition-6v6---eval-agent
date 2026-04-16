# Phase10.7 Critical-Head Ablation Report

## Scope and Constraints
- Goal: recover critical_event_accuracy with minimal changes, while freezing Phase10.6 termination semantics and eval protocol.
- Frozen items: termination semantics, heldout_pack, gate rules, reward normalization, backbone, visualization layer.
- Comparable protocol: all runs use heldout_cf strict eval and heldout counterfactual eval with horizons=20,terminal, state_step=0, min_groups=8, min_pairs=100.

## Experiments
- E0: Phase10.6 final baseline artifacts (reused only).
- E1: stage2 critical_hard_positive_weight=1.0.
- E2: stage2 critical group weights up (first_kill/objective/termination).
- E3: stage2 hard_positive + stronger critical group weights.

## Commands (executed in this phase)
- E1 prepare checkpoint:
  - New-Item -ItemType Directory -Force -Path agent_mvp/data_world_model_cf/phase10_7_ablation/E1
  - Copy-Item -Force agent_mvp/data_world_model_cf/phase10_6_ft_nocf/model.pt agent_mvp/data_world_model_cf/phase10_7_ablation/E1/model.pt
- E1 train:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phase10_7_ablation/E1 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --stage2-loss-config-path agent_mvp/data_world_model_cf/phase10_7_ablation/configs/E1_stage2_loss.json
- E1 strict eval:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --checkpoint agent_mvp/data_world_model_cf/phase10_7_ablation/E1/model.pt --data-dir agent_mvp/data_world_model_cf/heldout_pack --splits heldout_cf --horizons 20,terminal --report-out agent_mvp/data_world_model_cf/phase10_7_ablation/E1_strict_eval.json
- E1 counterfactual eval:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --checkpoint agent_mvp/data_world_model_cf/phase10_7_ablation/E1/model.pt --counterfactual-data-dir agent_mvp/data_world_model_cf/heldout_pack --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --report-out agent_mvp/data_world_model_cf/phase10_7_ablation/E1_counterfactual_eval.json
- E2 prepare checkpoint:
  - New-Item -ItemType Directory -Force -Path agent_mvp/data_world_model_cf/phase10_7_ablation/E2
  - Copy-Item -Force agent_mvp/data_world_model_cf/phase10_6_ft_nocf/model.pt agent_mvp/data_world_model_cf/phase10_7_ablation/E2/model.pt
- E2 train:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phase10_7_ablation/E2 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --stage2-loss-config-path agent_mvp/data_world_model_cf/phase10_7_ablation/configs/E2_stage2_loss.json
- E2 strict eval:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --checkpoint agent_mvp/data_world_model_cf/phase10_7_ablation/E2/model.pt --data-dir agent_mvp/data_world_model_cf/heldout_pack --splits heldout_cf --horizons 20,terminal --report-out agent_mvp/data_world_model_cf/phase10_7_ablation/E2_strict_eval.json
- E2 counterfactual eval:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --checkpoint agent_mvp/data_world_model_cf/phase10_7_ablation/E2/model.pt --counterfactual-data-dir agent_mvp/data_world_model_cf/heldout_pack --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --report-out agent_mvp/data_world_model_cf/phase10_7_ablation/E2_counterfactual_eval.json
- E3 prepare checkpoint:
  - New-Item -ItemType Directory -Force -Path agent_mvp/data_world_model_cf/phase10_7_ablation/E3
  - Copy-Item -Force agent_mvp/data_world_model_cf/phase10_6_ft_nocf/model.pt agent_mvp/data_world_model_cf/phase10_7_ablation/E3/model.pt
- E3 train:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phase10_7_ablation/E3 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --stage2-loss-config-path agent_mvp/data_world_model_cf/phase10_7_ablation/configs/E3_stage2_loss.json
- E3 strict eval:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/eval_world_model.py --checkpoint agent_mvp/data_world_model_cf/phase10_7_ablation/E3/model.pt --data-dir agent_mvp/data_world_model_cf/heldout_pack --splits heldout_cf --horizons 20,terminal --report-out agent_mvp/data_world_model_cf/phase10_7_ablation/E3_strict_eval.json
- E3 counterfactual eval:
  - C:/Users/98466/.conda/envs/rainyll/python.exe agent_mvp/python/counterfactual_eval_world_model.py --checkpoint agent_mvp/data_world_model_cf/phase10_7_ablation/E3/model.pt --counterfactual-data-dir agent_mvp/data_world_model_cf/heldout_pack --splits heldout_cf --horizons 20,terminal --state-step 0 --min-groups 8 --min-pairs 100 --report-out agent_mvp/data_world_model_cf/phase10_7_ablation/E3_counterfactual_eval.json

## Strict Eval Key Metrics (heldout_cf)
- E0: critical=0.7525000080, term_acc=0.9625, term_high_conf=1.0, fire_mae=0.8076997295, reward_red_mae=2.9904629052, reward_total_mae=2.9900743991, terminal_win=0.55
- E1: critical=0.7400000077, term_acc=0.9625, term_high_conf=1.0, fire_mae=0.6856260871, reward_red_mae=9.6441227376, reward_total_mae=10.1462666556, terminal_win=0.5875
- E2: critical=0.7400000077, term_acc=0.9625, term_high_conf=1.0, fire_mae=0.6912306322, reward_red_mae=8.8325797528, reward_total_mae=9.4332803205, terminal_win=0.5625
- E3: critical=0.7400000077, term_acc=0.9625, term_high_conf=1.0, fire_mae=0.6929504011, reward_red_mae=8.9952668756, reward_total_mae=9.4975734204, terminal_win=0.525

## Counterfactual Focus Metrics (heldout_cf)
- Coverage validity:
  - E0/E1/E2/E3 all valid with groups=16 and pairs=160.
- termination_flag signal_pairs:
  - E0=12, E1=12, E2=12, E3=12 (non-zero preserved in all ablations).
- termination_flag sign_accuracy_on_signal_pairs:
  - E0=0.25, E1=0.5, E2=0.5, E3=0.5.
- event_red_fire_count sign_accuracy_on_signal_pairs:
  - E0=0.5573770492, E1=0.6065573770, E2=0.6393442623, E3=0.5901639344.
- reward_red sign_accuracy_on_signal_pairs:
  - E0=0.6101694915, E1=0.4915254237, E2=0.5423728814, E3=0.4745762712.
- terminal_red_win_prob sign_accuracy_on_signal_pairs:
  - E0=0.6818181818, E1=0.6590909091, E2=0.6818181818, E3=0.6704545455.

## Decision
- Main objective status: NOT achieved.
- Why:
  - None of E1/E2/E3 recovered critical_event_accuracy; all are 0.74, below E0=0.7525000080.
  - Strict reward errors regressed heavily in all ablations (reward_red_mae and reward_total_mae both much worse than E0).
  - Positive: termination semantics behavior remained stable (strict termination metrics unchanged) and counterfactual termination signal_pairs stayed non-zero.
- Best candidate under damage control: E2.
- Go/No-Go: NO-GO to replace E0 with any E1/E2/E3 checkpoint.

## Artifact Checklist
- E1: model.pt, meta.json, train_history.json, E1_strict_eval.json, E1_counterfactual_eval.json
- E2: model.pt, meta.json, train_history.json, E2_strict_eval.json, E2_counterfactual_eval.json
- E3: model.pt, meta.json, train_history.json, E3_strict_eval.json, E3_counterfactual_eval.json
