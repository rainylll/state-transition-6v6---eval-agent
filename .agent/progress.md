# 当前任务进度

## 当前主线
Phase S：用 self / non-self terminal master interface + self_derived gate 压住 terminal `blue_first_kill` confusion，并判断它能否晋升为新主版本候选。

## 当前阶段
已完成到 `Phase S.4`。

## 最近结论
- `Phase S` 路线已经确认有效，不是一次性 lucky run。
- `S.2` 把坏 seed 从灾难态拉回，说明 safe-band gate 稳定化有效。
- `S.3` 发现主矛盾已经从“负侧压不住”转成“正侧会不会一起压没”。
- `S.4` 结论：
  - 单纯 positive-side relief 不能救回 `seed11` 的 heldout 零正例。
  - positive-side protection 能救回 `seed11`，但会让 `seed19` 的 terminal confusion 重新恶化。
  - 目前还没有同时满足“救回 `seed11` 且稳住 `seed19`”的 promotion-safe 配置。

## 当前最佳已知配置
- 路线：self / non-self terminal master interface
- terminal first-kill 接口：`self_derived`
- 当前最稳的 gate 配置：`S3_w80_6535`
  - `self_gate_stability_weight = 80`
  - `self_gate_positive_margin = 0.65`
  - `self_gate_negative_margin = 0.35`
- 这一配置下：
  - `seed7`、`seed19` 很强
  - `seed11` 会出现 heldout `blue_first_kill_pred_positive_rate = 0.0`

## 最新关键产物
- 最佳稳态参考：`agent_mvp/data_world_model_cf/phaseS3_safe_band_local_tuning_report.md`
- 正侧保护实验：`agent_mvp/data_world_model_cf/phaseS4_positive_side_gate_report.md`
- 当前相关训练入口：`agent_mvp/python/train_world_model.py`
- 当前相关评估入口：`agent_mvp/python/eval_world_model.py`

## 下一步
- 进入“主版本候选晋升裁决”。
- 裁决问题：
  - Phase S 是否已足够替换 `10.8`
  - 还是继续保留为第一主路线，但暂不升主版本

## 本轮待验证项
- 是否接受 `S3_w80_6535` 作为“最稳但不完美”的候选
- 是否允许再做一次极窄的单点插值，例如 `positive_scale 1.10 ~ 1.15`
- 如果不再追加实验，则应直接做晋升裁决
