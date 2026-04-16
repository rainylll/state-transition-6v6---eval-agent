# Phase 10.5 A/B 验证报告（Held-out Counterfactual）

## 评估设置
- Model A: Phase7 functional baseline
- Model B: Phase10.5 retrain（train_visible_cf + CF finetune）
- Held-out pack: ../data_world_model_cf/heldout_pack
- Counterfactual gate: min_groups=8, min_pairs=100（A/B 均 valid）

## Strict Eval（heldout_cf）
- trajectory_alive_accuracy: A=0.8708333333333333 | B=0.9083333333333332
- terminal_win_accuracy: A=0.475 | B=0.525
- critical_event_accuracy: A=0.7950000070035458 | B=0.7725000087171793
- termination_flag_accuracy: A=1.0 | B=1.0
- termination_flag_high_conf_hit_rate: A=1.0 | B=1.0
- event_first_fire_accuracy: A=0.85625 | B=0.85625
- event_fire_count_mae: A=1.1009429537982214 | B=0.7838523497863207
- reward_red_mae: A=71.1013928413391 | B=2.9964044868946074
- reward_total_mae: A=65.83648985028267 | B=3.014285999536514
- reward_red_delta_scale: A=78.05485545396805 | B=5.949282878637314
- reward_red_true_delta_scale: A=6.953462612628937 | B=6.953462612628937

## Held-out Counterfactual Eval（heldout_cf）
- groups/pairs: A=16/160 | B=16/160
- valid: A=True | B=True
- event_red_fire_count: delta_mae A=1.0277445227373392 | B=1.0253706853138282; sign_acc A=0.45901639344262296 | B=0.5573770491803278; signal_pairs=61
- termination_flag: delta_mae A=1.7149845348285453e-06 | B=1.7255929378734437e-07; sign_acc A=None | B=None; signal_pairs=0
- reward_red: delta_mae A=0.46392205404117703 | B=0.4590221271850169; sign_acc A=0.6101694915254238 | B=0.6101694915254238; signal_pairs=59
- terminal_red_win_prob: delta_mae A=0.5535499071702361 | B=0.5501008929684759; sign_acc A=0.3181818181818182 | B=0.6818181818181818; signal_pairs=88

## 结论
- Phase10.5 路线在 reward 与 counterfactual 区分能力上出现明确提升，且未出现 trajectory/terminal 的同步退化。
- 仍存在 tradeoff：critical_event_accuracy 低于 baseline，需要对关键事件头做定向修补。
- 建议继续沿 Phase10.5 深入，但下一步应聚焦关键事件命中与高置信校准。