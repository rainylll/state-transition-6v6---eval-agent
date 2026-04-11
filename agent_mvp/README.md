# 宏观评估 Agent MVP

本目录提供 `海空战对抗宏观评估 Agent 项目规划书.md` 的首个可运行版本。

当前版本目标是 **“先跑通全流程”**：
- 从场景采样到模型预测形成完整流水线。
- 支持变长、异构实体集合输入。
- 输出胜率与战损相关估计结果。

## 目录说明

- `configs/scenario_grid.yaml`：场景采样范围与数据集切分比例。
- `../docs/Simulation_Data_Contract.md`：黑盒仿真输入/输出契约（唯一标准）。
- `../README.md`：项目总览、三层架构与核心流转机制说明。
- `python/build_dataset.py`：生成原始与 train/val/test 切分后的 JSONL 数据。
- `python/train.py`：训练多任务基线模型。
- `python/eval.py`：计算测试指标。
- `python/predict_once.py`：单条样本推理。
- `python/tactic_slicer.py`：宏观兵力切分为局部模板评估的骨架实现。
- `python/real_adapter.py`：将真实引擎 JSON 适配为模型样本格式。
- `python/test_real_adapter_compat.py`：适配器与张量接口兼容性检查。
- `python/ingest_real_episodes.py`：将 C++ 导出对局转换为 train/val/test JSONL。
- `python/smoke_test.py`：最小端到端冒烟脚本。
- `docs/v1_scope.md`：V1 范围边界（包含/不包含）。
- `docs/roadmap.md`：质量迭代路线图。

## 快速开始

```powershell
python agent_mvp\python\build_dataset.py --config agent_mvp\configs\scenario_grid.yaml --out-dir agent_mvp\data --episodes 5000 --seed 7
python agent_mvp\python\train.py --data-dir agent_mvp\data --out-dir agent_mvp\artifacts --epochs 20 --batch-size 64 --lr 0.001 --device cpu
python agent_mvp\python\eval.py --data-dir agent_mvp\data --model-path agent_mvp\artifacts\model.pt --out-path agent_mvp\artifacts\metrics.json --device cpu
python agent_mvp\python\predict_once.py --model-path agent_mvp\artifacts\model.pt --input agent_mvp\examples\sample_case.json --output agent_mvp\artifacts\prediction.json --device cpu
```

## 冒烟测试

```powershell
python agent_mvp\python\smoke_test.py
```

## 真实数据适配检查

使用模拟的引擎风格 JSON，验证 Adapter 输出可直接喂给注意力模型。

```powershell
python agent_mvp\python\test_real_adapter_compat.py
python agent_mvp\python\predict_once.py --model-path agent_mvp\artifacts\model.pt --input agent_mvp\examples\mock_real_case.json --input-format real --output agent_mvp\artifacts\prediction_from_real.json --device cpu
```

## C++ 导出数据摄取

```powershell
python agent_mvp\python\validate_episodes.py --input-path agent_mvp\examples\mock_cpp_export_episodes.jsonl --fail-on P0
python agent_mvp\python\ingest_real_episodes.py --input-path agent_mvp\examples\mock_cpp_export_episodes.jsonl --out-dir agent_mvp\data_real --seed 7
python agent_mvp\python\train.py --data-dir agent_mvp\data_real --out-dir agent_mvp\artifacts_real --epochs 2 --batch-size 4 --device cpu
```

## 说明

- 当前标签由 `python/simulator_stub.py` 生成，用于先把流程跑通。
- 下一阶段可用 C++ 无头仿真导出数据替换该 stub。
- 当前基线为 DeepSets 风格（池化 + MLP），后续可升级为 Set Transformer。
- 推理接口使用 `python/inference_api.py::evaluate_tactic`，返回：
  `red_win_prob`、`red_expected_survival`、`blue_expected_survival`，以及可选的 `red_expected_ammo_used`。


