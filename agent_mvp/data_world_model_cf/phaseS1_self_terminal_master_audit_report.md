# Phase S.1 Reproduction, Mirror Check, and Gate Audit

## Commands
- A_eval_heldout: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/heldout_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseS1_A_10_8_heldout_strict.json --splits heldout_cf --device cpu --terminal-self-master-mode none`
- A_eval_terminal_confusion: `python agent_mvp/python/eval_world_model.py --data-dir agent_mvp/data_world_model_cf/terminal_confusion_pack --model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --out-path agent_mvp/data_world_model_cf/phaseS1_A_10_8_terminal_confusion.json --splits terminal_confusion_primary_cf --device cpu --terminal-self-master-mode none`
- S_seed7_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_derived`
- S_seed11_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_seed11 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 11 --terminal-self-master-mode self_derived`
- S_seed19_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_v1_seed19 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 19 --terminal-self-master-mode self_derived`
- ablation_head_only_seed7_train: `python agent_mvp/python/train_world_model.py --data-dir agent_mvp/data_world_model_cf/ab_train_visible_only --out-dir agent_mvp/data_world_model_cf/phaseS_self_terminal_master_head_only_seed7 --epochs 0 --stage2-epochs 1 --stage2-lr 0.00015 --init-model-path agent_mvp/data_world_model_cf/phase10_8_firstkill_fix/model.pt --stage2-firstkill-focus --stage2-firstkill-positive-repeat 6 --stage2-firstkill-hard-negative-repeat 3 --device cpu --seed 7 --terminal-self-master-mode self_head_only`

## Multi-Seed Summary (self_derived)
| metric | A | seed7 | seed11 | seed19 |
| --- | --- | --- | --- | --- |
| heldout::event_first_kill_accuracy | 0.7 | 0.84375 | 0.8375 | 0.6375 |
| heldout::critical_event_accuracy | 0.7650000086054206 | 0.8225000068545342 | 0.8200000086799264 | 0.740000007674098 |
| heldout::blue_first_kill_accuracy | 0.7125 | 0.925 | 0.925 | 0.5875 |
| heldout::blue_first_kill_precision | 0.23333333333333334 | 0.6 | 0.6 | 0.175 |
| heldout::termination_flag_accuracy | 0.9625 | 0.9625 | 0.9625 | 0.9625 |
| heldout::reward_red_mae | 3.1467823565006254 | 3.1896230280399323 | 3.1204550087451937 | 3.110897696018219 |
| heldout::reward_total_mae | 3.074378550052643 | 3.1452891498804094 | 3.0700075387954713 | 3.0766232192516325 |
| terminal::blue_first_kill_false_positive_rate | 0.7 | 0.06666666666666667 | 0.06666666666666667 | 1.0 |
| terminal::event_first_kill_accuracy | 0.4 | 0.75 | 0.65 | 0.25 |
| terminal::critical_event_accuracy | 0.560000017285347 | 0.700000015894572 | 0.6600000182787578 | 0.5000000149011612 |

## Red/Blue Mirror Audit
- S_seed7: terminal red-view acc=0.5 | blue-view acc=0.0 | red non-self mean FK prob=0.49443697134653725 | blue non-self mean FK prob=0.48381693263848624
- S_seed11: terminal red-view acc=0.5 | blue-view acc=0.0 | red non-self mean FK prob=0.5147948682308197 | blue non-self mean FK prob=0.48081593910853065
- S_seed19: terminal red-view acc=0.5 | blue-view acc=0.0 | red non-self mean FK prob=0.5735767364501954 | blue non-self mean FK prob=0.543042020003001

## Minimal Ablation
- head_only seed7: terminal role acc=0.75 | blue-view acc=1.0 | blue FP=0.8333333333333334
- self_derived seed7: terminal role acc=0.25 | blue-view acc=0.0 | blue FP=0.06666666666666667

## Verdict
- Two of three seeds reproduce the big blue-side gain; one seed collapses back into all-positive terminal blue first-kill gating.
- The current mechanism is better described as a fragile but powerful probability gate, not a stable terminal role classifier.