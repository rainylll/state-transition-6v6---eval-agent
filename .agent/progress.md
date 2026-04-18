# Current Task Progress

## Official Project State
- `10.8` = formal stable main version
- Original `Phase S (S4.1 / 1.10)` candidate passed `P1`
- Original `Phase S (S4.1 / 1.10)` candidate failed `P2` on mirror inconsistency
- Promotion was paused
- One final no-retrain mirror-interface repair was allowed
- `R1` passed

## Official Candidate Status
- Official candidate name: `phaseR1_candidate_s4_1_110_mirror_coupled`
- Internal review status: `Conditional Go`
- Promotion status: resumed with documented caveat
- Official status line:
  - `Promotion resumed for the R1 mirror-coupled decision-interface candidate; upstream terminal_self_role gate asymmetry remains a documented caveat.`
- Scope of resumed promotion:
  - the R1 mirror-coupled decision-interface candidate
  - not the raw pre-R1 candidate by itself

## Why Promotion Resumed
- P1 main surface stayed intact across seeds `7 / 11 / 19`
- Blue-facing terminal FPR stayed at `0.0667`
- Exported terminal `first_kill` mirror inconsistency was materially reduced on the patched decision layer
- R1 achieved this with a no-retrain derivation-layer repair only

## Documented Caveat
- The exported terminal `first_kill` decision interface is now mirror-coupled
- The upstream `terminal_self_role` gate is still not fully mirror-symmetric
- This caveat must remain attached to the promoted candidate

## Historical Materials Index
- Expected historical P1 report paths are not present in the current working tree:
  - `agent_mvp/data_world_model_cf/phaseP1_promotion_report.md`
  - `agent_mvp/data_world_model_cf/phaseP1_promotion_report.json`
- Expected historical P2 report paths are not present in the current working tree:
  - `agent_mvp/data_world_model_cf/phaseP2_mirror_gate_report.md`
  - `agent_mvp/data_world_model_cf/phaseP2_mirror_gate_report.json`
- Current authoritative R1 audit materials are present at:
  - `agent_mvp/data_world_model_cf/phaseR1_mirror_coupled_report.md`
  - `agent_mvp/data_world_model_cf/phaseR1_mirror_coupled_report.json`
  - `agent_mvp/data_world_model_cf/phaseR1_mirror_gate_check.json`
  - `agent_mvp/data_world_model_cf/phaseR1_final_internal_decision.md`
  - `agent_mvp/data_world_model_cf/phaseR1_final_internal_decision.json`

## Artifact Mapping
- Official candidate name: `phaseR1_candidate_s4_1_110_mirror_coupled`
- Frozen weight artifacts remain at:
  - `agent_mvp/data_world_model_cf/phaseS4_1_posscale110_seed7/model.pt`
  - `agent_mvp/data_world_model_cf/phaseS4_1_posscale110_seed11/model.pt`
  - `agent_mvp/data_world_model_cf/phaseS4_1_posscale110_seed19/model.pt`

## Boundary For The Next Handoff
- Promotion can continue from the R1 mirror-coupled candidate state
- Do not reopen patch-sea local tuning from here
- Do not erase the upstream terminal_self_role caveat in later summaries
