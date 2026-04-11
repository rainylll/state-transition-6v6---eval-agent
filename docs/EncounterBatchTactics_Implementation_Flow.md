# EncounterBatchTactics.cpp 实现流程详解

> 这份文档描述的是当前默认 demo/smoke-test tactic 的实现流程，用于全流程验证和联调参考；它不是战术团队唯一的实现方式。战术团队接入时，请优先参考 `agent_mvp/cpp_sandbox_template/` 下的 template 文件，以及 `docs/Tactical_Team_Integration_Guide.md`。

## 1. 文档目标

本文面向 C++ 战术开发与联调人员，详细说明 `agent_mvp/cpp_sandbox_template/EncounterBatchTactics.cpp` 的实现流程、状态机切换逻辑与关键阈值。

### 1.1 联调口径快照（验收用）

- 当前 `EncounterBatchTactics.*` 是 demo/smoke-test tactic，用于全流程验证。
- 战术团队接入的参考入口是 template（`EncounterBatchRunner_Template.cpp` + `EncounterBatchTactics_Template.*`）。
- 战术团队只需在 `agent_mvp/cpp_sandbox_template/` 内接入规则/RL 实现。
- runner 负责读取任务、调用战术、输出 `episodes.jsonl`（模板 runner 默认输出 `episodes_template.jsonl`，可用 `--out-path` 覆盖）。
- Python 侧继续负责 `merge / ingest / train / eval`，正式联调链路仍以 demo runner 产物 `agent_mvp/data_real/raw/episodes.jsonl` 为准。

说明范围：
- 战术层的输入、处理、输出。
- 编队分工（LEAD/WING/DECOY/ESCORT/RTB/EVADE）如何切换。
- 威胁评估、规避机动、协同齐射、任务完成判定。

不包含范围：
- 底层物理推进（`EnvStep`）和场景初始化细节（由 Runner/引擎侧负责）。
- `simulation_tasks.jsonl` 与 `episodes.jsonl` 的合并（由 `agent_mvp/python/merge_tasks_and_episodes.py` 负责）。

---

## 2. 文件职责与边界

`EncounterBatchTactics.cpp` 的职责可以概括为：

1. 每个阵营维护一个 `TeamTacticController`。
2. 基于当前态势动态选择目标、分配角色、规划航迹。
3. 根据威胁触发躲避，必要时覆盖原角色为 `EVADE`。
4. 控制主攻机在合适距离协同开火。
5. 将战术态势写入 `_pilot`，用于 Tacview 标签显示。

该文件不负责：
- 物理状态积分。
- 场景时间推进。
- JSON 输入输出。

---

## 3. 在 Runner 中的调用时序

该战术模块由 `EncounterBatchRunner.cpp` 驱动，典型调用顺序如下：

```text
构造 red_tactic / blue_tactic
-> Initialize()
-> 每步循环:
   RefreshIfNeeded()
   UpdateObjective()
   Apply()
   EnvStep(dt)
-> 循环结束后读取 objective_complete()/GetTelemetry()
```

循环中的关键点：
- `RefreshIfNeeded()` 用于重规划目标与几何。
- `UpdateObjective()` 判定是否达成任务结束条件。
- `Apply()` 写入本步控制指令与 Tacview 标签。

---

## 4. 关键数据结构

### 4.1 TeamMissionPlan（队级任务计划）

核心字段：
- `target_id`：当前攻击目标。
- `team_plane_ids`：本方存活飞机 ID 列表。
- `primary_attackers`：主攻机列表（最多 2 架）。
- `decoys`：诱饵机列表。
- `escorts`：护航机列表。
- `selection_anchor`：战术几何参考点（由主攻或全队中心计算）。
- `target_reference`：最近一次规划时目标参考点。
- `return_anchor`：返航锚点。
- `profile`：机动剖面参数（弧度、半径、高度、开火距离等）。
- `objective_complete`：任务是否完成。

### 4.2 PlaneControlState（机级控制状态）

核心字段：
- `role`：当前角色。
- `fired_rounds`：该机已开火轮次。
- `cooldown_steps`：开火冷却步数。
- `last_threat`：最近威胁快照。
- `dodge`：规避状态（是否激活、剩余步数、规避锚点、目标高度与速度）。

### 4.3 ThreatSnapshot（威胁快照）

核心字段：
- `warning_count`：雷达告警数（来自 `_recvRadarWarnState`）。
- `inbound_missile_count`：来袭导弹数（敌机导弹状态中目标为本机）。
- `nearest_missile_distance_m`：最近威胁距离。
- `warning_origin_distance_m`：告警源距离。
- `severe`：是否严重威胁。

---

## 5. 初始化阶段

入口：`TeamTacticController::Initialize()`。

执行顺序：
1. 清空并重置 `plan_`。
2. 统计本队初始总弹量 `initial_team_missiles`。
3. 调用 `RefreshPlan(..., reason="init")` 完成第一次目标选择与几何规划。

初始化后即可得到：
- 初始目标。
- 主攻/诱饵/护航编组。
- 首轮战术剖面与返航锚点。

---

## 6. 几何与目标自适应刷新

入口：`RefreshIfNeeded()`。

刷新触发条件：
1. 当前目标无效或死亡：立即重新选最近敌机并重规划（`retarget`）。
2. 若本队已开火，短期内不频繁重规划。
3. 若距离上次刷新不足 120 步，不刷新。
4. 若目标漂移或主攻锚点漂移超过阈值，触发几何重规划（`geometry`）。

重规划由 `RefreshPlan()` 执行，输出：
- 新 `target_id`。
- 新 `selection_anchor / target_reference / return_anchor`。
- 新 `TacticProfile`。

---

## 7. 角色到底代表什么（先看结论）

这 6 个词不是“配置参数”，而是每个仿真步实时计算出的战术状态标签。

- LEAD：主攻编队中的第 1 架（主攻领机）。
- WING：主攻编队中的其余飞机（主攻僚机）。
- DECOY：诱饵机（通常在主攻机不足 2 架时由无弹机担任）。
- ESCORT：非主攻、非诱饵且当前有弹的飞机。
- RTB：返航状态（Return To Base）。
- EVADE：规避状态（被威胁触发，优先级高于其他角色）。

关键理解：角色是“当前行为意图 + 态势覆盖”的输出，不是固定身份。

### 7.1 逐角色详细说明（进入条件 / 主要行为 / 退出条件）

#### LEAD
进入条件：本机在 `primary_attackers` 且是第 1 架。  
主要行为：按主攻行为占位或直扑目标，满足条件后参与齐射。  
退出条件：触发规避后转为 EVADE；编组变化后不再是主攻机时可能转 RTB；任务完成时统一转 RTB。

#### WING
进入条件：本机在 `primary_attackers` 且不是第 1 架。  
主要行为：与 LEAD 同步执行主攻，占位角度不同。  
退出条件：触发规避后转为 EVADE；编组变化后离开主攻时可能转 RTB；任务完成时统一转 RTB。

#### DECOY
进入条件：`AssignRoles` 把本机放进 `decoys`。典型场景是主攻机只有 1 架时，由最近无弹机担任。  
主要行为：飞诱饵点牵制敌方，有告警时降速提升生存。  
退出条件：触发规避后转为 EVADE；重规划后不再是诱饵机时可能转 RTB；任务完成时统一转 RTB。

#### ESCORT
进入条件：本机非主攻、非诱饵且有弹。  
主要行为：当前实现中没有独立 Escort 行为控制器。  
退出条件：同一帧的行为分支会覆盖为 RTB；若规避激活则覆盖为 EVADE。

#### RTB
进入条件：本机非主攻且非诱饵（常见于无弹机），或任务完成后全员强制返航。  
主要行为：飞向 `return_anchor`。  
退出条件：下一帧若重分组进入主攻/诱饵，或触发规避，则切到其他角色。

#### EVADE
进入条件：`dodge.active = true`，由导弹威胁或持续雷达告警触发。  
主要行为：执行规避机动，朝规避锚点飞行并限速。  
退出条件：规避剩余步数耗尽且威胁消失，下一帧回到基础角色判定流程。

---

## 8. 角色如何分配（AssignRoles 产出的是分组，不是最终标签）

入口：`AssignRoles()`，每个战术步都会调用。

该函数只做三件事：
1. 找出本方存活飞机。
2. 根据“是否有弹 + 距目标距离”做分组。
3. 输出 `primary_attackers / decoys / escorts` 三个列表。

分组算法（严格按代码顺序）：
1. 把本方存活机分成有弹组与无弹组。
2. 有弹组按距目标从近到远排序。
3. 有弹组前 2 架放进 `primary_attackers`。
4. 其余有弹机放进 `escorts`。
5. 如果主攻机只有 1 架且存在无弹机：
   - 取最近无弹机作为 `decoy`。
   - 剩余无弹机进 `escorts`。
6. 否则全部无弹机直接进 `escorts`。

注意：到这里还没有最终显示的角色字符串，最终角色在 `Apply()` 内按优先级覆盖后才写入 Tacview。

---

## 9. 同一帧内的角色优先级与切换顺序

入口：`Apply()`。

一架飞机在同一帧里会经历“基础角色判定 -> 规避覆盖 -> 行为分支覆盖”的过程。最终写入标签的是最后一次赋值后的角色。

本帧优先级（从高到低）：
1. 任务已完成分支：全员直接 RTB（提前 return）。
2. 规避分支：若 `dodge.active`，强制改为 EVADE。
3. 常规行为分支：主攻走 Attack，诱饵走 Decoy，其余走 Return。
4. 基础角色分配：Decoy / Lead / Wing / Escort / RTB（这是最早一层）。

非常关键的实现细节：
- 虽然基础判定里会出现 ESCORT，
- 但在当前实现中，非主攻且非诱饵会进入 Return 行为分支，并再次把角色改成 RTB。
- 所以 ESCORT 在当前代码里通常是“中间态”，最终标签大多数情况下会显示 RTB 或 EVADE，而不是 ESCORT。

---

## 10. 威胁评估与 EVADE 覆盖机制

### 10.1 威胁评估（AnalyzeThreatForPlane）

数据来源：
1. `_recvRadarWarnState`：得到 `warning_count` 与告警源距离。
2. 敌机 `_missileState`：筛选目标为本机的在飞导弹，得到 `inbound_missile_count` 与最近威胁距离。

`severe` 触发条件（满足其一）：
1. 有来袭导弹且最近导弹距离 < 35km。
2. 来袭导弹数 > 1。
3. 告警数 > 1 且告警源距离 < 30km。

### 10.2 规避触发（MaybeActivateDodge）

两类触发：
1. 导弹触发：存在来袭导弹。
2. 雷达触发（门限按生存优先与否不同）：
   - 生存优先（诱饵）：`warning_count>0 && warning_streak>=30 && warning_origin_distance<=35km`
   - 非生存优先（主攻）：`warning_count>1 && warning_streak>=40 && warning_origin_distance<=26km`

触发后会设置：
1. `dodge.active = true`
2. `dodge.remaining_steps`（导弹威胁下更长）
3. `dodge.anchor`（规避锚点）
4. `dodge.desired_altitude / desired_speed`

### 10.3 规避执行（ApplyDodgeBehavior）

执行效果：
1. 强制 `state.role = EVADE`。
2. 用 `evade_controller` 飞向规避锚点。
3. 按规避速度目标压制油门上限。
4. 剩余步数耗尽且无威胁后退出规避。
5. 若快结束但威胁仍在，会续接规避。

---

## 11. 常规行为执行（非规避情况下）

### 11.1 主攻行为（Attack）

触发条件：`is_primary_attacker && target存在 && 本机有弹`。

执行要点：
1. 先算攻击点 `BuildAttackPoint()`。
2. 远离攻击窗口时先占位，进入窗口后直扑目标。
3. 已捕获目标时设置 `Track + targetID`，为后续发射做准备。

### 11.2 诱饵行为（Decoy）

触发条件：`is_decoy && target存在`。

执行要点：
1. 计算诱饵点 `BuildDecoyPoint()`（前推 + 侧向偏置）。
2. 有告警时进一步降速提高生存。

### 11.3 返航行为（Return）

触发条件：不满足主攻与诱饵条件，或任务已经完成。

执行要点：
1. 飞向 `return_anchor`。
2. 使用剖面高度和返航速度。
3. 角色最终显示为 RTB。

---

## 12. 协同齐射 + 最终标签如何形成

### 12.1 协同齐射（HandleVolleyFire）

只对 `primary_attackers` 执行齐射判定：
1. 飞机要满足存活、有弹、冷却结束、已捕获目标。
2. 开火阈值按轮次递减：
   - `fire_threshold = max(22km, first_fire_range - round_range_interval * fired_rounds)`
3. 双机场景下，如果只有一机 ready：
   - 会短暂等待另一机同步。
   - 若另一机规避中或威胁严重，可放行单机。
   - 等待超时也会放行单机。

发射动作：
1. `_radarState = Track`
2. `_targetID = plan_.target_id`
3. `_isShoot = true`
4. `fired_rounds++`
5. `cooldown_steps = 25`

### 12.2 每步最终标签生成

标签入口：`BuildTacviewLabel()`。

格式：`<Role> M<missile_count> W<warning_count> I<inbound_missile_count>`。

角色字符串映射：
1. `Lead -> LEAD`
2. `Wing -> WING`
3. `Decoy -> DECOY`
4. `Escort -> ESCORT`
5. `Return -> RTB`
6. `Evade -> EVADE`

其中：
1. `M` 是本机当前剩余导弹数。
2. `W` 是本机告警数（来自 `_recvRadarWarnState`）。
3. `I` 是来袭导弹数（敌机 `_missileState` 中目标为本机的在飞导弹数）。

### 12.3 一眼看懂的单机决策伪代码

```text
if objective_complete:
  role = RTB
  return_behavior
  write_label
  continue

base_role = (DECOY / LEAD / WING / ESCORT / RTB)
maybe_activate_dodge(base_role, threat)

if dodge.active:
  role = EVADE
  evade_behavior
else if primary_attacker and has_missile:
  role = LEAD or WING
  attack_behavior
else if decoy:
  role = DECOY
  decoy_behavior
else:
  role = RTB
  return_behavior

write_label(role, M, W, I)
```

---

## 13. 任务完成判定

入口：`UpdateObjective()`。

完成条件：
1. 敌方全灭：立即完成。
2. 本方导弹耗尽且本方存活机全部回到返航锚点半径内，并持续保持一定步数（`kMissionCompletionHoldSteps`）后完成。

完成后在 `Apply()` 中将全员置为 `RTB`。

---

## 14. tactic_id 对机动剖面的影响

入口：`BuildAdaptiveTacticProfile()`。

规则：
- `tactic_id` 为偶数：
  - 更紧凑扇区（约 50~95 度）
  - 相对更高空
  - 首发距离更长
- `tactic_id` 为奇数：
  - 更宽扇区（约 120~180 度）
  - 相对更低空
  - 首发距离更短

若可用主攻机不足 2 架，会进一步收紧参数以适配单机进攻。

---

## 15. 关键常量速查

- `kBattlefieldSquareSideMeters = 600000`：战场方格边长。
- `kBattlefieldEdgeMarginMeters = 60000`：边界安全内缩。
- `kReturnCompletionRadiusMeters = 15000`：返航完成半径。
- `kMissionCompletionHoldSteps = 20`：完成保持步数。
- `kFireReadyHoldTimeoutSteps = 45`：双机协同等待超时步数。
- `kInvalidThreatDistance = 1e12`：无效威胁距离哨兵值。

---

## 16. 一句话总结

`EncounterBatchTactics.cpp` 本质上是一个“每步重评估 + 角色动态切换 + 威胁覆盖规避 + 协同开火”的战术状态机：先定计划，再按威胁实时改角色与行为，最终通过 `_pilot` 把态势透明地输出到 Tacview。
