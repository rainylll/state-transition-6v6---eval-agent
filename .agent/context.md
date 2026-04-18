# 项目级上下文

## 一句话目标
本项目用于构建“海空战对抗宏观评估 Agent”工作流：生成任务、批量仿真、整理数据、训练评估模型，并把结果回接到后续 Agent / 业务评估链路。

## 核心流水线
任务生成 -> C++ 仿真批跑 -> 任务/回包合并 -> ingest / dataset -> 训练 -> 严格评估 -> counterfactual / confusion pack 评估 -> 报告与展示

## 主要目录
- `agent_mvp/python/`: 任务生成、ingest、dataset、训练、评估、报告脚本
- `agent_mvp/data_world_model_cf/`: counterfactual world model 数据、checkpoint、阶段报告
- `agent_mvp/data_world_model/`: 常规 world model 数据与报告
- `agent_mvp/data_real/`: 从 C++ 真实导出链路整理出的数据
- `agent_mvp/cpp_sandbox_template/`: C++ batch runner 与 tactic 模板
- `docs/`: 项目级契约与流程说明
- `Source/`: 底层 C++ 环境实现

## 核心脚本职责
- `generate_sim_tasks.py`: 生成仿真任务
- `merge_tasks_and_episodes.py`: 合并任务与仿真回包
- `ingest_real_episodes.py`: 真实回包 ingest
- `ingest_rollouts.py`: rollout / counterfactual 数据 ingest
- `world_model_dataset.py`: world model 数据集与 target 派生
- `world_model.py`: world model 网络结构
- `train_world_model.py`: world model 训练入口
- `eval_world_model.py`: heldout / terminal confusion 严格评估
- `counterfactual_eval_world_model.py`: counterfactual 评估
- `build_terminal_confusion_pack.py`: 构建 terminal confusion 评估包

## 长期稳定的架构认知
- 当前建模主线是 `world_model_*` 这套训练/评估链，而不是早期 `train.py / eval.py` baseline。
- 当前关键问题集中在 terminal horizon 的 first-kill 语义，而不是 backbone 表达能力不足。
- 主评估面以 heldout strict eval、terminal confusion pack、counterfactual eval 为主。
- `10.8` 仍是当前稳定基线版本；Phase S 是当前最强主候选路线，但尚未正式替换基线。

## 常用参考文档
- `README.md`
- `docs/Model_Training_and_Inference_Guide.md`
- `docs/Simulation_Data_Contract.md`
- `agent_mvp/data_world_model_cf/phase10_8_report.md`
