# 问题与失败知识库

## 当前主要问题
Phase S 的核心剩余矛盾不是 terminal 负侧压不住，而是：
- 正侧保护太弱，`seed11` 会失去 heldout 正例发射能力
- 正侧保护一旦增强到能救回 `seed11`，`seed19` 又会重新打开 terminal confusion

## 已知风险
- gate 机制目前仍是 calibration-dominant，不是 role argmax-dominant。
- 当前最危险的失败模式是正负两侧耦合过强，缺少稳定的安全带宽度。
- `seed19` 对 gate 位置仍敏感，容易重新贴近 `0.5` 边界。

## 已失败或不建议继续的路线
- `exclude / weighting / repeat patch` 继续扩展：已被明确叫停，不建议再开 patch 海。
- 弱并联辅助 role 头：没有成为主收益来源，不建议继续。
- budget sanity / budget sweep：当前阶段不建议做。
- `self_head_only`：role 看起来更好，但压不住 terminal blue confusion，不建议作为主线。
- 继续单纯增强 negative-side 压制：会更容易把正侧一起压没，不建议继续。
- 只做 positive-side relief：`seed11` 救不回来。

## S.4 的失败知识
- `positive_scale < 1.0`：
  - 会让 self / non-self 两侧概率一起下移
  - 不能救回 `seed11`
- `positive_scale > 1.0`：
  - 能救回 `seed11`
  - 但最先有效的版本就会让 `seed19` terminal FPR 明显回弹

## 明确不建议混淆的事情
- 不要把 `Phase S` 当前实验结论写回 `AGENTS.md`
- 不要把项目结构说明写进 bug 列表
- 不要把失败路线重新当成默认候选再跑一遍

## 当前未解决的关键矛盾
- 如何在不重新放开 `seed19` 的前提下，恢复 `seed11` 的正例发射能力
- 如何把 gate 从“窄平衡点”推进成“可稳定固化”的主版本机制
