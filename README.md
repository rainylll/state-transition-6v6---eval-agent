# 海空战宏观评估 Agent

这个仓库做的是一件很具体的事：

1. 先生成一批海空对抗初始态势任务。
2. 用 C++ 推演器批量跑这些任务，得到每局结果。
3. 把推演结果整理成模型可用的数据集。
4. 训练一个宏观评估模型，预测胜率、生存和战损。
5. 在后续业务或 Agent 决策里，快速调用这个模型做评估。

如果你是第一次接手这个项目，可以把它理解成一条固定的数据流水线：

`生成任务 -> C++ 推演 -> 合并回包 -> 数据处理 -> 训练 -> 评估`

---

## 1. 这个项目的输入和输出是什么

### 输入

AI 侧先生成 `simulation_tasks.jsonl`。每条任务里描述一局对抗的初始状态，核心是 7 维特征：

`[type_id, speed, sensor, initial_missile, lon, lat, initial_alive]`

其中：

- `type_id` 是作战单位种类编码，一个编号代表一个种类。
- `speed` 是初始速度。
- `sensor` 是传感器/雷达能力代理值。
- `initial_missile` 是初始带弹量。
- `lon` / `lat` 是初始经纬度。
- `initial_alive` 是初始是否存活。

### 输出

C++ 推演后会输出 `episodes.jsonl`。每条结果只保留轻量终态回包，核心是 2 维特征：

`[final_missile, final_alive]`

训练前会再把初始任务和终态结果按 `task_id` 合并成 `episodes_merged.jsonl`，然后进入 Python 数据处理和训练链路。

---

## 2. 仓库目录怎么认

最常用的目录和文件如下：

- `agent_mvp/python/`：任务生成、数据合并、数据处理、训练、评估脚本。
- `agent_mvp/cpp_sandbox_template/`：当前 demo/smoke-test tactic，以及给战术团队接入用的 template。
- `agent_mvp/data_real/raw/`：原始任务和原始回包。
- `agent_mvp/data_real/processed/`：训练前处理好的数据集。
- `agent_mvp/artifacts_real/`：训练产物和评估结果。
- `agent_mvp/data_real/replays/`：推演阶段输出的 `.acmi` 回放。
- `Source/`：底层 C++ 环境实现。
- `CMakeLists.txt`：C++ 构建入口。
- `build_runner.sh`：Linux 下的批量推演启动脚本。
- `build_runner.ps1`：Windows 下的批量推演启动脚本。
- `requirements.txt`：本项目维护的 Python 依赖文件。

### 2.1 仓库里有两类 C++ 战术内容

当前仓库故意把“全流程验证用的 demo 战术”和“给战术团队接入的模板”分开保存：

- 当前可运行的 demo/smoke-test tactic：
  `[EncounterBatchRunner.cpp](agent_mvp/cpp_sandbox_template/EncounterBatchRunner.cpp)` +
  `[EncounterBatchTactics.h](agent_mvp/cpp_sandbox_template/EncounterBatchTactics.h)` +
  `[EncounterBatchTactics.cpp](agent_mvp/cpp_sandbox_template/EncounterBatchTactics.cpp)`
- 给战术团队参考和复制改造的 template：
  `[EncounterBatchRunner_Template.cpp](agent_mvp/cpp_sandbox_template/EncounterBatchRunner_Template.cpp)` +
  `[EncounterBatchTactics_Template.h](agent_mvp/cpp_sandbox_template/EncounterBatchTactics_Template.h)` +
  `[EncounterBatchTactics_Template.cpp](agent_mvp/cpp_sandbox_template/EncounterBatchTactics_Template.cpp)`

这两套文件的定位不同：

- 如果你是架构、联调、训练侧，默认先看 demo 流程。当前默认 `EncounterBatchTactics.*` 的价值是验证“生成任务 -> C++ 跑批 -> Python merge/ingest/train/eval”闭环已经打通，不是战术团队未来实现的上限。
- 如果你是战术团队，重点看 template 和新的战术团队接入文档，不需要先读完整个 Python 训练链路。
- 战术团队最终只需要把自己的规则代码或 RL 推理代码放进 `agent_mvp/cpp_sandbox_template/`，并让 runner 稳定产出 `agent_mvp/data_real/raw/episodes.jsonl` 即可。

当前默认构建 target 仍然是 `EncounterBatchRunner`。额外提供的模板 target 是 `EncounterBatchRunnerTemplate`，用于让战术团队单独编译模板 runner，而不会影响现有 smoke-test 链路。

为避免模板调试时误覆盖正式产物，`EncounterBatchRunnerTemplate` 默认输出为 `agent_mvp/data_real/raw/episodes_template.jsonl`，并支持通过 `--out-path` 显式指定输出路径。

正式联调链路仍以 `build_runner.ps1` / `build_runner.sh` 产出的 `agent_mvp/data_real/raw/episodes.jsonl` 为准。

---

## 3. Linux 配环境最短步骤

下面这套命令默认你已经进入仓库根目录。

### 3.1 创建虚拟环境

```bash
conda create --name eval_agent python=3.11
conda activate eval_agent
python -m pip install --upgrade pip
```

### 3.2 先安装匹配 CUDA 的 PyTorch

这个项目训练默认建议用 `cuda`，但 PyTorch 的安装命令要跟你机器上的 CUDA 版本匹配，所以不写死进 `requirements.txt`。

请先根据你机器的 CUDA 版本，从 PyTorch 官方安装页选择对应命令。安装完后，用下面这条命令确认：

```bash
python -c "import torch; print(torch.__version__, torch.cuda.is_available())"
```

如果最后输出的 `torch.cuda.is_available()` 是 `False`，训练时把 `--device cuda` 改成 `--device cpu` 即可。

### 3.3 安装本仓库依赖

```bash
pip install -r requirements.txt
```

### 3.4 Linux 下 C++ 推演依赖

Linux 侧推荐准备这些基础工具：

- `cmake`
- `g++` 或 `clang++`
- `make` 或 `ninja`

首次在 Linux 上使用启动脚本前，建议执行：

```bash
chmod +x build_runner.sh
```

如果系统里没有 `nlohmann/json.hpp`，本仓库的 `CMakeLists.txt` 会尝试自动下载；离线环境下也可以提前把头文件路径写到环境变量里：

```bash
export NLOHMANN_JSON_INCLUDE=/path/to/json/include
```

---

## 4. Windows 和 Linux 的差异

这次仓库已经整理成两条都能工作的路径，但侧重点不同：

- Windows：保留现有完整链路，支持 `build_runner.ps1`、实时 TacView 遥测、WGUA 和摇杆相关路径。
- Linux：默认走无头批跑，不做实时 TacView 遥测；优先保证“能推演、能产出结果、能训练”。
- Linux 下仍然尽量保留 `.acmi` 文件回放输出，便于事后抽样观察。
- Windows 常用启动器是 `build_runner.ps1`，Linux 常用启动器是 `build_runner.sh`。

如果你的目标是“迁到 Linux 服务器上正式训练模型”，优先使用 Linux 这条无头链路即可。

---

## 5. 从零开始跑完整流程

下面所有命令都默认在仓库根目录执行。

### 5.1 生成初始任务

```bash
python agent_mvp/python/generate_sim_tasks.py \
  --out-path agent_mvp/data_real/raw/simulation_tasks.jsonl \
  --count 1000 \
  --seed 2026
```

产物：

- `agent_mvp/data_real/raw/simulation_tasks.jsonl`

### 5.2 批量推演

Linux：

```bash
./build_runner.sh
```

Windows：

```powershell
.\build_runner.ps1
```

这一步会做这些事：

1. 配置并编译 `EncounterBatchRunner`
2. 读取 `simulation_tasks.jsonl`
3. 批量推演每个任务
4. 输出 `episodes.jsonl`
5. 输出前 10 局左右的 `.acmi` 回放到 `agent_mvp/data_real/replays/`

Linux 默认是无头模式，不做实时 TacView 遥测。

这里默认跑的是当前 demo/smoke-test tactic，也就是 `EncounterBatchRunner.cpp + EncounterBatchTactics.*` 这一套。它的目的，是让架构、联调、训练侧先稳定拿到 `episodes.jsonl`，验证全流程闭环。战术团队后续如果接入自己的规则/RL 实现，推荐从 template 文件开始，而不是直接把当前 demo tactic 当作唯一标准样板。

产物：

- `agent_mvp/data_real/raw/episodes.jsonl`
- `agent_mvp/data_real/replays/*.acmi`

### 5.3 合并任务和回包

```bash
python agent_mvp/python/merge_tasks_and_episodes.py \
  --tasks-path agent_mvp/data_real/raw/simulation_tasks.jsonl \
  --episodes-path agent_mvp/data_real/raw/episodes.jsonl \
  --out-path agent_mvp/data_real/raw/episodes_merged.jsonl
```

产物：

- `agent_mvp/data_real/raw/episodes_merged.jsonl`

### 5.4 处理数据

```bash
python agent_mvp/python/ingest_real_episodes.py \
  --input-path agent_mvp/data_real/raw/episodes_merged.jsonl \
  --out-dir agent_mvp/data_real \
  --seed 7
```

产物：

- `agent_mvp/data_real/processed/train.jsonl`
- `agent_mvp/data_real/processed/val.jsonl`
- `agent_mvp/data_real/processed/test.jsonl`
- `agent_mvp/data_real/processed/summary.json`

### 5.5 训练模型

默认推荐先用 CUDA：

```bash
python agent_mvp/python/train.py \
  --data-dir agent_mvp/data_real \
  --out-dir agent_mvp/artifacts_real \
  --epochs 20 \
  --batch-size 64 \
  --lr 0.001 \
  --device cuda
```

如果你的机器没有可用 GPU，就改成：

```bash
python agent_mvp/python/train.py \
  --data-dir agent_mvp/data_real \
  --out-dir agent_mvp/artifacts_real \
  --epochs 20 \
  --batch-size 64 \
  --lr 0.001 \
  --device cpu
```

产物：

- `agent_mvp/artifacts_real/model.pt`
- `agent_mvp/artifacts_real/train_history.json`
- `agent_mvp/artifacts_real/meta.json`

### 5.6 评估模型

```bash
python agent_mvp/python/eval.py \
  --data-dir agent_mvp/data_real \
  --model-path agent_mvp/artifacts_real/model.pt \
  --out-path agent_mvp/artifacts_real/metrics.json \
  --device cuda
```

没有 GPU 时改成：

```bash
python agent_mvp/python/eval.py \
  --data-dir agent_mvp/data_real \
  --model-path agent_mvp/artifacts_real/model.pt \
  --out-path agent_mvp/artifacts_real/metrics.json \
  --device cpu
```

产物：

- `agent_mvp/artifacts_real/metrics.json`

---

## 6. 每一步结束后你应该看到什么

如果整条链路正常，最终至少会有这些文件：

- `agent_mvp/data_real/raw/simulation_tasks.jsonl`
- `agent_mvp/data_real/raw/episodes.jsonl`
- `agent_mvp/data_real/raw/episodes_merged.jsonl`
- `agent_mvp/data_real/processed/train.jsonl`
- `agent_mvp/data_real/processed/val.jsonl`
- `agent_mvp/data_real/processed/test.jsonl`
- `agent_mvp/artifacts_real/model.pt`
- `agent_mvp/artifacts_real/metrics.json`

批量推演时，终端还会实时打印类似下面的进度日志：

- `[TASK x/n]`
- `[TASK x/n][ROUND r]`
- 每局结束时的胜负、剩余带弹数和终止原因

---

## 7. 常见报错怎么排查

### 7.1 `torch.cuda.is_available()` 是 `False`

原因通常是 PyTorch 装成了 CPU 版本，或者 CUDA 版本不匹配。

做法：

1. 重新按 PyTorch 官方页面安装匹配你机器 CUDA 版本的 wheel。
2. 确认：

```bash
python -c "import torch; print(torch.__version__, torch.cuda.is_available())"
```

3. 如果当前机器确实没有 GPU，就把训练和评估命令中的 `--device cuda` 改成 `--device cpu`。

### 7.2 `cmake` 不存在

说明 C++ 构建工具没装好。先安装 `cmake` 和编译器，再执行：

```bash
cmake --version
```

### 7.3 `nlohmann/json.hpp` 找不到

在线环境下，CMake 会尝试自动下载。

离线环境下，请先准备头文件目录，然后执行：

```bash
export NLOHMANN_JSON_INCLUDE=/path/to/json/include
```

再重新运行 `./build_runner.sh`。

### 7.4 `episodes.jsonl` 没生成

优先检查：

1. `agent_mvp/data_real/raw/simulation_tasks.jsonl` 是否存在
2. `EncounterBatchRunner` 是否编译成功
3. 推演过程中终端是否已经打印到 `[TASK ...]`
4. 终端最后是否有明确的 C++ 运行错误

### 7.5 Linux 下看不到实时 TacView

这是当前设计使然，不是故障。

Linux 版本默认走无头推演，不启用实时 TacView 遥测。需要实时遥测时，请回到 Windows 链路使用 `build_runner.ps1`。

---

## 8. 一句最短上手路径

如果你已经配好了 Python 和 PyTorch，最短可执行路径就是：

```bash
python agent_mvp/python/generate_sim_tasks.py --out-path agent_mvp/data_real/raw/simulation_tasks.jsonl --count 1000 --seed 2026
./build_runner.sh
python agent_mvp/python/merge_tasks_and_episodes.py --tasks-path agent_mvp/data_real/raw/simulation_tasks.jsonl --episodes-path agent_mvp/data_real/raw/episodes.jsonl --out-path agent_mvp/data_real/raw/episodes_merged.jsonl
python agent_mvp/python/ingest_real_episodes.py --input-path agent_mvp/data_real/raw/episodes_merged.jsonl --out-dir agent_mvp/data_real --seed 7
python agent_mvp/python/train.py --data-dir agent_mvp/data_real --out-dir agent_mvp/artifacts_real --epochs 20 --batch-size 64 --lr 0.001 --device cuda
python agent_mvp/python/eval.py --data-dir agent_mvp/data_real --model-path agent_mvp/artifacts_real/model.pt --out-path agent_mvp/artifacts_real/metrics.json --device cuda
```

---

## 9. 进一步阅读

这四个文档分别负责更细的说明，README 不再重复展开：

- [数据契约说明](docs/Simulation_Data_Contract.md)
- [训练与推理执行手册](docs/Model_Training_and_Inference_Guide.md)
- [当前 demo 战术实现流程](docs/EncounterBatchTactics_Implementation_Flow.md)
- [战术团队接入手册](docs/Tactical_Team_Integration_Guide.md)
