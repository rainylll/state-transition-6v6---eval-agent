# 海空战黑盒仿真数据契约规范（C++ 团队唯一干活手册）

本文档是 C++ 战术仿真团队与 AI 团队之间唯一有效的数据契约说明。

---

## 1. 协作模式与兵力规模修改

### 1.1 黑盒协作模式

统一采用“AI 发包，C++ 跑批并回传”的流程：
1. AI 侧生成 `simulation_tasks.jsonl`（输入任务）。
2. C++ 侧逐行读取任务并执行单局仿真（规则/RL 均可）。
3. C++ 侧输出 `episodes.jsonl`（回收结果）。
4. AI 侧执行 `merge_tasks_and_episodes.py`，按 `task_id` 将 `simulation_tasks.jsonl` 与 `episodes.jsonl` 合并为 `episodes_merged.jsonl`。
5. AI 侧摄取 `episodes_merged.jsonl` 进入训练。

在初期全链路联调阶段，默认批量规模为 **1000 局**，优先保证验证效率与问题定位速度。

核心边界：
- 我们只发包初态 **7D**。
- 我们只收包终态 **2D**。

战术团队的工作边界也固定为：

- 只需要遵守 7D 输入 / 2D 输出契约
- 只需要把自己的代码放到 `agent_mvp/cpp_sandbox_template/`
- 由 runner 负责读取任务、调用战术、批量推演、写出 `episodes.jsonl`
- 启动脚本再把结果归档到 `agent_mvp/data_real/raw/episodes.jsonl`
- 不需要改 Python 的 `merge / ingest / train / eval`
- 不需要改数据后处理逻辑
- 不需要改契约本身，除非 AI 团队与战术团队重新约定

### 1.2 如何修改遭遇战兵力规模（3v2 / 4v2 / 6v6）

请直接修改 `agent_mvp/python/generate_sim_tasks.py` 顶部的 `USER_CONFIG_ZONE`：

- `red_template` 中有几个字典，就生成几个红方单位。
- `blue_template` 中有几个字典，就生成几个蓝方单位。

例如：
- 3v2：`red_template` 3 项，`blue_template` 2 项
- 4v2：`red_template` 4 项，`blue_template` 2 项
- 6v6：`red_template` 6 项，`blue_template` 6 项

**无需修改生成器主逻辑**，仅通过增删模板项即可自由切换兵力规模。

默认快速联调模板采用“红蓝作战单位同构、兵力数量可不对称”的初始化方式：
- 红蓝两侧默认都使用同一种作战单位
- 同一种作战单位在红蓝两侧应使用同一个 `type_id`
- 红蓝差异主要通过单位数量、初始位置、导弹数随机采样体现

当前默认生成器会把红蓝初始位置限制在一个统一的大方格内，方格参数参考 `Source\Tools\RedCommander.cpp` 与 `Source\Tools\BlueCommander.cpp` 中的基地经纬度：
- 方格中心约为 `118.0 / 21.577106`
- 方格边长约为 `600 km`
- 距离边线的最小安全距离约为 `60 km`
- 红方默认在方格西半区随机采样，蓝方默认在东半区随机采样

另外，`EncounterBatchRunner` 在读入 7D 初态后还会再做一次边界夹紧；即使旧任务文件里带有越界经纬度，也会被压回同一个安全方格中。

### 1.3 一键式批处理执行器说明

当前仓库同时提供两套 C++ 战术相关文件：

- 全流程验证用 demo/smoke-test tactic：
  `EncounterBatchRunner.cpp + EncounterBatchTactics.h/.cpp`
- 给战术团队接入用的 template：
  `EncounterBatchRunner_Template.cpp + EncounterBatchTactics_Template.h/.cpp`

根目录 `CMakeLists.txt` 默认生成 `EncounterBatchRunner`，并额外提供 `EncounterBatchRunnerTemplate`。Windows 下由 `build_runner.ps1` 负责联调时的数据复制、终端日志输出、运行与结果归档；Linux 下由 `build_runner.sh` 负责同样的批处理流程。Linux 默认走无头批跑，不启用实时 Tacview 遥测。

推荐理解方式：

- 架构/联调/训练侧默认看 demo runner，先保证全流程闭环打通
- 战术团队默认看 template runner 和 template tactics，在 `agent_mvp/cpp_sandbox_template/` 中接入自己的规则/RL 逻辑
- 战术团队不需要改 Python 训练链路，只需要保证最终能生成符合契约的 `agent_mvp/data_real/raw/episodes.jsonl`
- 模板 runner 默认输出 `agent_mvp/data_real/raw/episodes_template.jsonl`，并支持 `--out-path`，用于避免覆盖 demo 产物
- 正式联调链路仍以 demo runner 产物 `agent_mvp/data_real/raw/episodes.jsonl` 为准

它已经封装了：
- `simulation_tasks.jsonl` 的逐行读取
- 固定 1000 条任务的批处理循环
- 7D 初态向量解析
- `episodes.jsonl` 的结果回写
- 训练前可直接执行 `python agent_mvp\python\merge_tasks_and_episodes.py`，按 `task_id` 生成 `agent_mvp/data_real/raw/episodes_merged.jsonl`
- 首 10 局 `.acmi` 回放文件输出到 `agent_mvp/data_real/replays/`
- 终端实时输出 `[TASK x/n][ROUND r]` 进度日志
- runner 主流程只负责初始化、引擎同步与战术模块调用
- 战术模块会读取底层 `_recvRadarWarnState` 与敌机 `_missileState`，实现齐射、诱饵、躲弹与返航控制
- 对当前随机初始化场景，战术模块会根据敌我实时相对几何动态调整进攻侧、攻击点、返航点与首发距离
- Tacview 回放中会动态显示飞机角色、剩余导弹数、告警数与来袭导弹数
- 这里“角色”是战术分工角色（`LEAD/WING/DECOY/ESCORT/RTB/EVADE`），不是雷达模式；雷达模式请看 Tacview 的 `RadarMode` 字段
- 标签格式示例：`LEAD M2 W1 I0`，其中 `M`=剩余导弹数，`W`=雷达告警数（`_recvRadarWarnState`），`I`=来袭导弹数（敌机 `_missileState` 中以本机为目标的在飞导弹数）
- 推演不再按固定 120 秒硬停，而是以“一方完成目标”或“一方被歼灭”为主终止条件，并保留 `9000` 轮安全上限

C++ 战术团队只需补两件事：
1. 把 7D 数据对接到你们实体结构体。
2. 在独立战术文件中调用你们自己的战术执行逻辑，并回填 2D 终态结果。

如果你是战术团队，更推荐直接从 `EncounterBatchRunner_Template.cpp` 与 `EncounterBatchTactics_Template.*` 开始，而不是把当前 `EncounterBatchTactics.*` 当成唯一实现标准。当前默认 `EncounterBatchTactics.*` 的价值，是让项目先稳定验证“发包 -> 推演 -> 回包 -> merge -> ingest -> train -> eval”这一条链路。

如需离线构建，请预先设置 `NLOHMANN_JSON_INCLUDE` 指向包含 `nlohmann/json.hpp` 的目录；在线环境下 CMake 会在本地未命中时自动获取该头文件依赖。

---

## 2. 输入发包契约（7D）

每个实体初态向量固定为：

`[type_id, speed, sensor, initial_missile, lon, lat, initial_alive]`

JSONL 示例（每行一条）：

```json
{
  "task_id": "sim_task_00001",
  "tactic_id": 1,
  "initial_state": {
    "red_features": [[0.0, 300.0, 120.0, 4.0, 116.850, 22.100, 1.0]],
    "blue_features": [[0.0, 300.0, 120.0, 4.0, 119.350, 22.400, 1.0]]
  }
}
```

字段要求：
- `task_id`：任务唯一标识。
- `tactic_id`：战术模板 ID。
- `initial_state.red_features` / `initial_state.blue_features`：7D 数组列表。

### 2.1 `type_id` 字段说明

`type_id` 是**作战单位的种类编码**，不是阵营编码；一个编号代表一个种类。

约束如下：
- 同一种作战单位在红蓝两侧应使用同一个 `type_id`
- 不同种类的作战单位才使用不同 `type_id`
- `type_id` 的数值映射必须由 Python 与 C++ 共享同一套约定

当前仓库默认快速联调模板中，红蓝双方都使用 `type_id = 0.0`，表示同种类作战单位对抗。

### 2.2 7D 各字段语义

- 第 1 维 `type_id`：作战单位种类编码
- 第 2 维 `speed`：初始速度
- 第 3 维 `sensor`：传感器/雷达量程代理值
- 第 4 维 `initial_missile`：初始导弹数
- 第 5 维 `lon`：初始经度
- 第 6 维 `lat`：初始纬度
- 第 7 维 `initial_alive`：初始存活标记

---

## 3. 输出回收契约（2D）

每个实体终态向量固定为：

`[剩余导弹, 最终存活]`

JSONL 示例（每行一条）：

```json
{
  "task_id": "sim_task_00001",
  "outcome": { "red_win": 1 },
  "final_state": {
    "red_features": [[1.0, 1.0]],
    "blue_features": [[5.0, 0.0]]
  }
}
```

字段要求：
- `outcome.red_win`：胜负标签（0/1）
- `final_state.red_features` / `final_state.blue_features`：2D 数组列表，数量与初态对齐

说明：
- `episodes.jsonl` 默认是轻量回包（`task_id + outcome + final_state`），不重复携带完整 `initial_state`。
- 训练前应先用合并脚本补齐初态字段，生成 `episodes_merged.jsonl`。

推荐命令：

```powershell
python agent_mvp\python\merge_tasks_and_episodes.py --tasks-path agent_mvp\data_real\raw\simulation_tasks.jsonl --episodes-path agent_mvp\data_real\raw\episodes.jsonl --out-path agent_mvp\data_real\raw\episodes_merged.jsonl
```

---

## 4. 规则与 RL 数值统一兼容（重点）

C++ 端无需准备两套格式，规则与 RL 共用同一 2D 数组契约。

### 4.1 规则推演输出（整数）

可直接输出确定值：
- `[1, 0]`：剩 1 弹，阵亡
- `[2, 1]`：剩 2 弹，存活

### 4.2 RL 推演输出（浮点期望）

可直接输出期望值：
- `[1.2, 0.15]`：预期剩 1.2 弹，存活率 15%
- `[1.5, 0.8]`：预期剩 1.5 弹，存活率 80%

### 4.3 Python 端自动兼容

Python 摄取端会将 2D 数组统一解析为浮点并进入 `float32` 张量链路：
- `ingest_real_episodes.py`：按 `float(...)` 解析 int/float
- `dataset.py`：构建 `torch.tensor(..., dtype=torch.float32)`

结论：C++ 端只管把数值填入 2D 数组，整数/小数都可。

