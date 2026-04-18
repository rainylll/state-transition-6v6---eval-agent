# Project Context

## One-Line Goal
Build and evaluate a tactical world-model pipeline for simulated combat state transitions, strict heldout evaluation, terminal-confusion checks, and downstream analysis.

## Core Pipeline
task generation -> simulation -> ingest -> dataset -> train -> strict eval -> terminal confusion eval -> counterfactual / audit -> reports

## Key Directories
- `agent_mvp/python/`: ingest, dataset, model, train, eval, reporting scripts
- `agent_mvp/data_world_model_cf/`: counterfactual / heldout packs, checkpoints, phase reports
- `docs/`: stable project-level docs
- `.agent/`: agent memory split into context / progress / bugs

## Stable Architecture Facts
- The project's main modeling line is the `world_model_*` training and evaluation pipeline.
- Current high-value evaluation surfaces are:
  - `heldout_pack`
  - `terminal_confusion_primary_cf`
  - mirror / gate diagnostics
- The important terminal issue has been about exported `first_kill` semantics more than trunk capacity.

## Official Versioning State
- `10.8` remains the formal stable main version.
- Official promoted research candidate name is:
  - `phaseR1_candidate_s4_1_110_mirror_coupled`
- Current internal review state:
  - `Conditional Go / promotion resumed with documented caveat`
- Official status line:
  - `Promotion resumed for the R1 mirror-coupled decision-interface candidate; upstream terminal_self_role gate asymmetry remains a documented caveat.`
- This promoted candidate is defined by:
  - the frozen `S4.1 / 1.10` weights
  - plus the R1 no-retrain mirror-coupled terminal decision interface

## Historical Materials Index
- The current working tree does not contain the expected P1/P2 summary report files at:
  - `agent_mvp/data_world_model_cf/phaseP1_promotion_report.md`
  - `agent_mvp/data_world_model_cf/phaseP1_promotion_report.json`
  - `agent_mvp/data_world_model_cf/phaseP2_mirror_gate_report.md`
  - `agent_mvp/data_world_model_cf/phaseP2_mirror_gate_report.json`
- The current authoritative material set for status recovery is:
  - `phaseR1_mirror_coupled_report.md`
  - `phaseR1_mirror_coupled_report.json`
  - `phaseR1_mirror_gate_check.json`
  - `phaseR1_candidate_s4_1_110_mirror_coupled_manifest.json`
  - `phaseR1_candidate_s4_1_110_mirror_coupled_note.md`
  - `phaseR1_final_internal_decision.md`
  - `phaseR1_final_internal_decision.json`
