# 战术团队接入手册

这份文档面向“只负责战术实现的人”。如果你的工作重点是规则战术、强化学习策略、模型推理接入，而不是 Python 训练链路或整个仓库的工程维护，那么优先看这份文档。

---

## 1. 这个项目整体在做什么

从宏观上看，这个项目是一条闭环链路：

```text
AI 生成 simulation_tasks.jsonl
-> C++ runner 批量执行单局推演
-> 输出 episodes.jsonl
-> Python merge / ingest / train / eval
-> 业务侧或 Agent 侧加载模型做宏观评估
```

你可以把它理解成：

- 上游 AI 负责“发包”
- C++ 战术侧负责“把一局局任务跑完并回包”
- Python 侧负责“把回包转成训练数据并训练模型”

---

## 2. 战术团队在整个项目中的职责是什么

战术团队只负责“单局推演中的战术决策逻辑”。

更具体地说，你们负责：

- 读取 runner 传入的态势输入
- 生成每一步的控制指令
- 让底层环境推进
- 最终让 runner 得到符合契约的终态结果

你们不负责：

- 管理整个仓库
- 改 Python 数据处理链路
- 改训练代码
- 改评估代码
- 改在线推理 API

---

## 3. 你们真正要改哪些文件

重点目录只有一个：

- `agent_mvp/cpp_sandbox_template/`

当前仓库里有两套 C++ 战术相关文件：

### 3.1 当前可运行 demo/smoke-test tactic

- `agent_mvp/cpp_sandbox_template/EncounterBatchRunner.cpp`
- `agent_mvp/cpp_sandbox_template/EncounterBatchTactics.h`
- `agent_mvp/cpp_sandbox_template/EncounterBatchTactics.cpp`

这套代码的价值是：

- 先验证全流程已经打通
- 先让架构/联调/训练侧稳定拿到 `episodes.jsonl`
- 先保证 `merge -> ingest -> train -> eval` 可以继续跑

它不是你们未来正式实现的上限，也不是唯一写法。

### 3.2 给战术团队接入的 template

- `agent_mvp/cpp_sandbox_template/EncounterBatchRunner_Template.cpp`
- `agent_mvp/cpp_sandbox_template/EncounterBatchTactics_Template.h`
- `agent_mvp/cpp_sandbox_template/EncounterBatchTactics_Template.cpp`

推荐你们的工作方式是：

1. 保留 template runner 的编排结构
2. 在 template tactics 中填入自己的规则逻辑，或接入 RL 推理逻辑
3. 如果需要，也可以复制 template 再改出你们自己的版本，但仍建议保留“runner 负责编排、战术文件负责决策”的边界

---

## 4. 你们不需要改哪些东西

至少下面这些，不是战术团队这次工作的重点：

- 不需要改 Python 的 `merge_tasks_and_episodes.py`
- 不需要改 Python 的 `ingest_real_episodes.py`
- 不需要改 Python 的 `train.py`
- 不需要改 Python 的 `eval.py`
- 不需要理解全部业务层
- 不需要改数据契约本身，除非双方重新约定
- 不需要为了接战术而改整条 Windows 默认链路或 Linux 无头链路

一句话说：你们只管在 `agent_mvp/cpp_sandbox_template/` 里接入战术，Python 训练链路继续按原样消费结果。

---

## 5. runner 和战术模块是怎么配合的

推荐按下面这个时序理解：

```text
读取 simulation_tasks.jsonl
-> 解析 7D 初态
-> 初始化环境实体
-> 进入逐步循环
-> 每步刷新态势
-> 调用战术模块
-> 写入控制指令
-> EnvStep 推进一步
-> 判断是否结束
-> 收集终态结果
-> 写 episodes.jsonl
```

边界非常清楚：

- runner 负责读任务、解析 7D、初始化环境、逐步同步、收集终态、写结果
- 战术模块负责看当前态势，然后产生命令

在 template runner 里，这个边界已经用注释写清楚了。

---

## 6. 你们最终的交付标准是什么

对战术团队来说，最终交付标准至少包括：

- 代码能被 runner 调用
- 能消费 `simulation_tasks.jsonl`
- 能在底层环境里稳定跑完单局和批量任务
- 能输出符合契约的 `episodes.jsonl`
- 上游可以继续跑 `merge -> ingest -> train`

换句话说，战术层的交付不是“某个类写完了”，而是“这套战术已经能放进当前 runner 编排里，持续产出正确回包”。

---

## 7. 输入输出契约简版

### 7.1 输入：7D 初态

每个单位的初态特征为：

`[type_id, speed, sensor, initial_missile, lon, lat, initial_alive]`

其中你们最需要关心的是：

- `task_id`
- `initial_state.red_features`
- `initial_state.blue_features`

### 7.2 输出：2D 终态

每个单位的终态特征为：

`[final_missile, final_alive]`

最终结果至少包含：

- `task_id`
- `outcome.red_win`
- `final_state.red_features`
- `final_state.blue_features`

只要这些字段符合契约，Python 侧就能继续 merge、ingest、train、eval。

---

## 8. 规则 / RL 两种实现如何接入

template 已经把这两类接法的边界预留出来了。

### 8.1 规则实现

最直接的方式是：

- 在 `EncounterBatchTactics_Template.cpp` 里写规则逻辑
- 根据当前单位态势、敌我距离、带弹数、告警等状态生成控制命令

这类实现通常会替换：

- `BuildRuleBasedCommands(...)`

### 8.2 RL 推理实现

如果你们要接训练好的 RL 模型，推荐方式是：

- 在 tactic 模块中加载模型
- 每个 step 把当前态势转成模型输入
- 让模型输出动作或控制参数
- 再回填成 runner 能理解的命令

template 里已经预留了：

- `UseRlPolicy()`
- `BuildRlInferenceCommand(...)`

不管是规则还是 RL，都应该通过同一个 runner 出结果，而不是重新绕开当前批处理入口另起一套 I/O。

---

## 9. 最小自检清单

在把代码交给架构/联调/训练侧之前，建议至少确认下面这些：

- 能编译
- 能跑 1 局
- 能批跑
- 能产出 `episodes.jsonl`
- `task_id` 能对齐
- `final_state.red_features` / `blue_features` 的数量能和初态对齐
- `outcome.red_win` 存在
- Python `merge_tasks_and_episodes.py` 不报错
- Python `ingest_real_episodes.py` 不报错

如果这些都通过，说明你们的战术已经满足接入当前训练链路的最小条件。

---

## 10. Windows / Linux 运行入口

当前项目保留两条入口：

- Windows：`build_runner.ps1`
- Linux：`build_runner.sh`

补充说明：

- Linux 默认走无头批跑
- Linux 侧当前目标是优先保证批处理链路，不强调实时遥测
- Windows 默认链路继续保留实时 Tacview、WGUA 等现有能力

如果你们只是开发战术本体，先在自己熟悉的平台把战术接进 template target 即可；最终再交给架构/联调侧放进默认链路做全流程验证。

---

## 11. 当前 demo 和 template 的关系

请把这句话记牢：

- 当前默认 `EncounterBatchTactics.*` 是 demo/smoke-test tactic
- 它的价值是验证架构闭环，不是战术团队最终实现的样板上限
- 战术团队真正应该参考的是 template 和这份接入文档

因此最推荐的协作方式是：

- 架构/联调/训练侧继续使用当前 demo 链路
- 战术团队在 `agent_mvp/cpp_sandbox_template/` 中基于 template 接入自己的规则/RL 代码
- 模板 runner 默认输出 `agent_mvp/data_real/raw/episodes_template.jsonl`（可用 `--out-path` 覆盖）
- 进入正式联调链路时，仍以 demo runner 产物 `agent_mvp/data_real/raw/episodes.jsonl` 为准
- Python 侧继续执行 `merge / ingest / train / eval`

---

## 12. 如何编译 template runner

当前仓库额外提供了一个独立 target：

- `EncounterBatchRunnerTemplate`

Windows 示例：

```powershell
cmake -S . -B build\encounter-runner
cmake --build build\encounter-runner --config Release --target EncounterBatchRunnerTemplate
```

Linux 示例：

```bash
cmake -S . -B build/encounter-runner
cmake --build build/encounter-runner --config Release --target EncounterBatchRunnerTemplate
```

默认启动脚本 `build_runner.ps1` / `build_runner.sh` 仍然编译和运行当前 demo target：`EncounterBatchRunner`。这能保证现有 smoke-test runner 不被战术模板工作流干扰。
