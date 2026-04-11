# 模型训练与联调手册（AI 算法团队 + LLM 后端团队）

本文档记录当前仓库已经落地可用的完整执行链路。推荐从项目根目录的 VS Code 集成终端启动，确保终端已经自动激活项目解释器环境后，再按本文顺序执行。

---

## 0. 执行顺序总览

当前推荐的数据流如下：

```text
生成 7D 初态任务
-> C++ 跑批，输出 2D 终态
-> 按 task_id 合并 tasks 与 episodes，生成 episodes_merged
-> Python 摄取与对齐，构建 processed 数据集
-> 训练 Set Transformer 宏观评估模型
-> 测试集评估与指标落盘
-> LLM/业务侧加载模型做在线战术评估
```

对应的标准命令顺序如下：

```powershell
python agent_mvp\python\generate_sim_tasks.py --out-path agent_mvp\data_real\raw\simulation_tasks.jsonl --count 1000 --seed 2026
.\build_runner.ps1
python agent_mvp\python\merge_tasks_and_episodes.py --tasks-path agent_mvp\data_real\raw\simulation_tasks.jsonl --episodes-path agent_mvp\data_real\raw\episodes.jsonl --out-path agent_mvp\data_real\raw\episodes_merged.jsonl
python agent_mvp\python\ingest_real_episodes.py --input-path agent_mvp\data_real\raw\episodes_merged.jsonl --out-dir agent_mvp\data_real --seed 7
python agent_mvp\python\train.py --data-dir agent_mvp\data_real --out-dir agent_mvp\artifacts_real --epochs 20 --batch-size 64 --lr 0.001 --device cpu
python agent_mvp\python\eval.py --data-dir agent_mvp\data_real --model-path agent_mvp\artifacts_real\model.pt --out-path agent_mvp\artifacts_real\metrics.json --device cpu
```

Linux 迁移补充：

- Linux 下的 Stage 2 启动器是 `./build_runner.sh`，不是 `.\build_runner.ps1`。
- Linux 默认走无头批跑，不启用实时 Tacview 遥测。
- 如果 Linux 机器已经装好 CUDA 版 PyTorch，训练和评估阶段建议把 `--device` 改成 `cuda`。

---

## 1. 前置条件

### 1.1 工作目录

所有命令默认都在项目根目录执行：

```powershell
D:\sky\Projects\state-transition 6v6 + eval agent
```

### 1.2 Python 环境

- 推荐在 VS Code 集成终端中执行，当前工作区会自动激活项目解释器。
- 若需手动检查解释器，请执行：

```powershell
python -c "import sys; print(sys.executable)"
```

### 1.3 C++ 构建依赖

Stage 2 现已由根目录 `CMakeLists.txt` 驱动构建。Windows 下使用 `build_runner.ps1`，Linux 下使用 `build_runner.sh`。其中 Linux 默认是无头批跑，不启用实时 Tacview 遥测。脚本会自动：

- 查找 Visual Studio C++ 工具链
- 调用 CMake 配置并构建 `EncounterBatchRunner`
- 复制运行时依赖
- 在终端输出构建、输入准备、批跑与归档日志

如果是离线环境，且本地没有可用的 `nlohmann/json.hpp`，请预先设置：

```powershell
$env:NLOHMANN_JSON_INCLUDE="你的 json.hpp 所在目录"
```

---

## 2. Stage 1：生成仿真任务

### 2.1 作用

由 Python 侧批量生成 7D 初始态任务，供 C++ 跑批器消费。

单个实体的初态特征为：

```text
[type_id, speed, sensor, initial_missile, lon, lat, initial_alive]
```

### 2.2 标准命令

```powershell
python agent_mvp\python\generate_sim_tasks.py --out-path agent_mvp\data_real\raw\simulation_tasks.jsonl --count 1000 --seed 2026
```

### 2.3 产物

- `agent_mvp\data_real\raw\simulation_tasks.jsonl`

### 2.4 可调项

- `--count`：生成任务数
- `--seed`：随机种子
- 若要修改编队规模、平台模板、初始态范围，请直接编辑 `agent_mvp\python\generate_sim_tasks.py` 中的 `USER_CONFIG_ZONE`
- `type_id` 是作战单位的种类编码，不是阵营编码；一个编号代表一个种类
- 红蓝如果是同一种作战单位，应使用同一个 `type_id`
- 当前默认模板使用红蓝同种类作战单位，双方默认都采用 `type_id = 0.0`
- 当前默认位置采样使用一个参考 `Source` 基地经纬度构造出的统一大战场方格
- 方格中心约为 `118.0 / 21.577106`，边长约为 `600 km`
- 采样时会为边界预留约 `60 km` 安全距离，红方默认落在西半区，蓝方默认落在东半区

---

## 3. Stage 2：C++ 推演跑批

### 3.1 作用

消费 `simulation_tasks.jsonl`，调用底层空战环境进行批量推演，并输出 `episodes.jsonl`。当前 Stage 2 已改成“runner 编排 + 独立战术模块”结构：`EncounterBatchRunner.cpp` 只负责批处理、初始化、引擎同步、日志和结果归档；实际战术放在 `agent_mvp/cpp_sandbox_template/EncounterBatchTactics.h/.cpp`。该战术模块会读取底层 `_recvRadarWarnState` 与敌机 `_missileState`，在随机初始化场景下动态分配主攻机、僚机、诱饵机与返航机，实现双机齐射、单机主攻/单机诱饵、躲弹机动与返航控制，并把 `角色 + 带弹数 + 告警数 + 来袭导弹数` 以 `Pilot/Label` 形式持续写入 Tacview。这里的“角色”指战术分工角色（`LEAD/WING/DECOY/ESCORT/RTB/EVADE`），不是雷达模式；雷达模式仍由 Tacview 的 `RadarMode` 字段独立显示。标签示例 `LEAD M2 W1 I0` 中，`M`=剩余导弹数，`W`=雷达告警数，`I`=来袭导弹数。平台初始化也已经尽量对齐 `Source` 的默认语义；`InitEnv` 之后会重新回写 7D 合同字段对应的速度、雷达量程、雷达波束和导弹数。

对 AI/训练侧来说，Stage 2 只需要关心一件事：runner 能否稳定产出 `agent_mvp/data_real/raw/episodes.jsonl`。当前默认接入的 `EncounterBatchTactics.*` 是一个 demo/smoke-test tactic，只用于打通链路，不是战术团队未来正式实现的上限。后续战术团队可以在 `agent_mvp/cpp_sandbox_template/` 中替换成自己的规则/RL 实现；Python 侧不依赖战术内部细节，只依赖 `episodes.jsonl` 是否持续满足契约。

终态回包采用 2D 压缩表示：

```text
[final_missile, final_alive]
```

### 3.2 首次完整执行

```powershell
.\build_runner.ps1
```

Linux 对应命令：

```bash
./build_runner.sh
```

该脚本会依次完成：

1. 用 CMake 配置 `build\encounter-runner`
2. 编译 `EncounterBatchRunner`
3. 将 `agent_mvp\data_real\raw\simulation_tasks.jsonl` 拷贝到项目根目录作为运行输入
4. 清理并重建回放目录 `agent_mvp\data_real\replays`
5. 在终端持续输出任务输入、当前推演到第几局、局内 `[ROUND r]` 进度、逐局结果与耗时
6. 将生成的 `episodes.jsonl` 归档到 `agent_mvp\data_real\raw\episodes.jsonl`
7. 首 10 局 `.acmi` 回放中会附带同一个大战场边界框，并在机体标签中持续显示角色和带弹数量，便于直接观测初始化是否越界、谁在主攻、谁在诱饵、谁正在躲弹

补充说明：

- 这里默认编译并运行的是 demo target：`EncounterBatchRunner`
- 仓库还额外提供了战术团队模板 target：`EncounterBatchRunnerTemplate`
- AI 团队通常不需要改这个模板 target，只需要确认默认链路的 `episodes.jsonl -> merge -> ingest -> train -> eval` 稳定可跑
- 模板 target 默认输出为 `agent_mvp/data_real/raw/episodes_template.jsonl`，并支持 `--out-path`，用于避免覆盖 demo 链路产物
- 正式联调与训练仍以 demo runner 产物 `agent_mvp/data_real/raw/episodes.jsonl` 为准

### 3.3 终端日志说明

执行启动脚本后，终端会看到：

- 构建配置、架构、Runner 路径
- `simulation_tasks.jsonl` 的检测行数
- 回放目录路径与回放文件数
- `EncounterBatchRunner` 的逐任务日志
- `EncounterBatchRunner` 的局内 `[TASK x/n][ROUND r]` 实时推演轮次日志
- `EncounterBatchTactics` 的 `[TACTIC] ... fire / evade / adapt` 事件日志
- 总批耗时与最终归档结果

`EncounterBatchRunner` 会为每个任务输出一行摘要，包含：

- 当前任务序号
- 当前局内推演轮次
- 当前是否正在输出回放
- `task_id`
- 红蓝单位数
- `red_win`
- 双方最终存活数
- 双方剩余导弹数
- 目标完成状态与终止原因
- 单局耗时

当前终止规则也已改成：
- 优先以“一方完成目标”结束
- 若一方被歼灭，则立即结束
- 若异常长时间未结束，才会落到 `9000` 轮安全上限

### 3.4 常用重跑方式

只重新构建，不运行仿真：

```powershell
.\build_runner.ps1 -SkipRun
```

Linux：

```bash
./build_runner.sh --skip-run
```

只重新跑仿真，不重新构建：

```powershell
.\build_runner.ps1 -SkipBuild
```

Linux：

```bash
./build_runner.sh --skip-build
```

切到 Debug：

```powershell
.\build_runner.ps1 -Configuration Debug
```

Linux：

```bash
./build_runner.sh --configuration Debug
```

### 3.5 产物

- `agent_mvp\data_real\raw\episodes.jsonl`
- `agent_mvp\data_real\replays\*.acmi`

默认会输出前 10 局的 `.acmi` 回放文件，用于快速抽样观测推演过程。Tacview 中每架飞机的 `Pilot/Label` 会动态显示如 `LEAD M2 W1 I0` 这类标签，分别表示当前角色、剩余导弹数、告警数和来袭导弹数。

---

## 4. Stage 3：任务回包合并 + 数据摄取、对齐与降维

### 4.1 作用

先将 C++ 输出的 `episodes.jsonl` 与 `simulation_tasks.jsonl` 按 `task_id` 合并为 `episodes_merged.jsonl`，再整理为模型训练所需的 `train/val/test` 数据集。

这里会完成两件关键工作：

- 将初态 7D 与终态 2D 对齐
- 将包含坐标的真实态势压缩为模型使用的 5D 连续特征

模型侧连续特征为：

```text
[speed, sensor, initial_missile, initial_alive, relative_distance]
```

`type_id` 不混入连续向量，而是进入 embedding 支路。

### 4.2 标准命令

```powershell
python agent_mvp\python\merge_tasks_and_episodes.py --tasks-path agent_mvp\data_real\raw\simulation_tasks.jsonl --episodes-path agent_mvp\data_real\raw\episodes.jsonl --out-path agent_mvp\data_real\raw\episodes_merged.jsonl
python agent_mvp\python\ingest_real_episodes.py --input-path agent_mvp\data_real\raw\episodes_merged.jsonl --out-dir agent_mvp\data_real --seed 7
```

### 4.3 可选的 4v2 切片评估命令

```powershell
python agent_mvp\python\merge_tasks_and_episodes.py --tasks-path agent_mvp\data_real\raw\simulation_tasks.jsonl --episodes-path agent_mvp\data_real\raw\episodes.jsonl --out-path agent_mvp\data_real\raw\episodes_merged.jsonl
python agent_mvp\python\ingest_real_episodes.py --input-path agent_mvp\data_real\raw\episodes_merged.jsonl --out-dir agent_mvp\data_real --seed 7 --expand-4v2 --evaluate-slices --slicer-strategy distance_threat
```

### 4.4 产物

- `agent_mvp\data_real\raw\episodes_merged.jsonl`
- `agent_mvp\data_real\processed\train.jsonl`
- `agent_mvp\data_real\processed\val.jsonl`
- `agent_mvp\data_real\processed\test.jsonl`
- `agent_mvp\data_real\processed\summary.json`

---

## 5. Stage 4：模型训练

### 5.1 作用

训练宏观评估模型，输出推理所需权重和训练元信息。

### 5.2 标准命令

```powershell
python agent_mvp\python\train.py --data-dir agent_mvp\data_real --out-dir agent_mvp\artifacts_real --epochs 20 --batch-size 64 --lr 0.001 --device cpu
```

### 5.3 训练日志

训练过程中会按 epoch 输出：

- `train_loss`
- `val_acc`
- `val_mse`

验证集精度提升时，会自动刷新 `model.pt`。

### 5.4 产物

- `agent_mvp\artifacts_real\model.pt`
- `agent_mvp\artifacts_real\train_history.json`
- `agent_mvp\artifacts_real\meta.json`

---

## 6. Stage 5：测试集评估

### 6.1 作用

加载训练出的模型，在 `processed/test.jsonl` 上计算最终指标，并将结果写入 `metrics.json`。

### 6.2 标准命令

```powershell
python agent_mvp\python\eval.py --data-dir agent_mvp\data_real --model-path agent_mvp\artifacts_real\model.pt --out-path agent_mvp\artifacts_real\metrics.json --device cpu
```

### 6.3 输出指标

当前脚本默认输出：

- `accuracy`
- `loss_mse`

### 6.4 产物

- `agent_mvp\artifacts_real\metrics.json`

---

## 7. Stage 6：在线推理与业务侧调用

### 7.1 作用

业务层或 LLM 侧加载训练好的模型，对给定战术进行快速宏观评估，得到：

- 红方胜率
- 双方存活期望
- 双方弹药消耗期望

### 7.2 Python 调用示例

```python
from pathlib import Path
import random

from agent_mvp.python.inference_api import InferenceEngine

engine = InferenceEngine(
    model_path=Path("agent_mvp/artifacts_real/model.pt"),
    device="cpu",
)

payload = {
    "red_units": [
        {"id": "R1", "type_id": 0, "features": [300.0, 120.0, 4.0, 1.0, 0.35]},
        {"id": "R2", "type_id": 0, "features": [290.0, 110.0, 3.0, 1.0, 0.35]},
    ],
    "blue_units": [
        {"id": "B1", "type_id": 0, "features": [300.0, 120.0, 4.0, 1.0, 0.35]},
    ],
    "tactic": [0.2, 0.7, 0.6, 0.4, 0.5],
}

outcome = engine.evaluate_tactic(payload, input_format="sample")
print(outcome)

red_win = 1 if random.random() < outcome["red_win_prob"] else 0
red_alive_next = [1 if random.random() < p else 0 for p in outcome["red_expected_survival"]]
blue_alive_next = [1 if random.random() < p else 0 for p in outcome["blue_expected_survival"]]

red_ammo_now = [4.0, 3.0]
blue_ammo_now = [5.0]

red_ammo_used = outcome.get("red_expected_ammo_used") or [0.0 for _ in red_ammo_now]
blue_ammo_used = outcome.get("blue_expected_ammo_used") or [0.0 for _ in blue_ammo_now]

red_ammo_next = [max(0.0, a - u) for a, u in zip(red_ammo_now, red_ammo_used)]
blue_ammo_next = [max(0.0, a - u) for a, u in zip(blue_ammo_now, blue_ammo_used)]

print({
    "red_win": red_win,
    "red_alive_next": red_alive_next,
    "blue_alive_next": blue_alive_next,
    "red_ammo_next": red_ammo_next,
    "blue_ammo_next": blue_ammo_next,
})
```

### 7.3 联调建议

- 对比不同战术时，尽量使用同一批初态任务，避免混入环境随机性。
- 需要可复现实验时，生成任务和 ingest 都固定种子。
- 若上游直接传 real payload，可尝试 `input_format="auto"` 或 `"real"`。

---

## 8. 推荐的一键联调顺序

下面这组命令就是当前项目“从数据生成到最终评估”的推荐执行顺序：

```powershell
python agent_mvp\python\generate_sim_tasks.py --out-path agent_mvp\data_real\raw\simulation_tasks.jsonl --count 1000 --seed 2026
.\build_runner.ps1
python agent_mvp\python\merge_tasks_and_episodes.py --tasks-path agent_mvp\data_real\raw\simulation_tasks.jsonl --episodes-path agent_mvp\data_real\raw\episodes.jsonl --out-path agent_mvp\data_real\raw\episodes_merged.jsonl
python agent_mvp\python\ingest_real_episodes.py --input-path agent_mvp\data_real\raw\episodes_merged.jsonl --out-dir agent_mvp\data_real --seed 7
python agent_mvp\python\train.py --data-dir agent_mvp\data_real --out-dir agent_mvp\artifacts_real --epochs 20 --batch-size 64 --lr 0.001 --device cpu
python agent_mvp\python\eval.py --data-dir agent_mvp\data_real --model-path agent_mvp\artifacts_real\model.pt --out-path agent_mvp\artifacts_real\metrics.json --device cpu
```

Linux 版本把第二步改成 `./build_runner.sh`；如果机器已经装好 CUDA 版 PyTorch，训练和评估阶段可把 `--device cpu` 改成 `--device cuda`。

执行完成后，重点检查以下文件是否都生成成功：

- `agent_mvp\data_real\raw\simulation_tasks.jsonl`
- `agent_mvp\data_real\raw\episodes.jsonl`
- `agent_mvp\data_real\raw\episodes_merged.jsonl`
- `agent_mvp\data_real\processed\train.jsonl`
- `agent_mvp\data_real\processed\val.jsonl`
- `agent_mvp\data_real\processed\test.jsonl`
- `agent_mvp\artifacts_real\model.pt`
- `agent_mvp\artifacts_real\train_history.json`
- `agent_mvp\artifacts_real\meta.json`
- `agent_mvp\artifacts_real\metrics.json`

---

## 9. 常见问题排查

### 9.1 `build_runner.ps1` 报 C++ 构建错误

- 确认已安装 Visual Studio C++ workload
- 确认 `cmake` 在 `PATH` 中可用
- 若提示找不到 `nlohmann/json.hpp`，请设置 `NLOHMANN_JSON_INCLUDE`

Linux 下对应检查：

- 确认使用的是 `./build_runner.sh`
- 确认系统里有 `cmake` 和 `g++` / `clang++`
- Linux 默认不启用实时 Tacview、WGUA 和摇杆路径

### 9.2 跑批没有生成 `episodes.jsonl`

- 先确认 `agent_mvp\data_real\raw\simulation_tasks.jsonl` 已存在
- 再确认终端里的逐任务日志是否已经正常打印；如果运行器刚启动就退出，优先看第一条红字错误
- 若 `EncounterBatchRunner` 进程异常退出，请优先看终端红字和根目录下调试 `.acmi`

### 9.3 训练阶段报找不到 processed 数据

说明 Stage 3（merge + ingest）尚未成功执行，或者 `--out-dir` 与训练命令的 `--data-dir` 不一致。

### 9.4 评估阶段报找不到模型

说明 `train.py` 尚未产出 `agent_mvp\artifacts_real\model.pt`，或训练输出目录与评估命令不一致。
