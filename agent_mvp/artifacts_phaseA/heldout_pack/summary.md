# Held-out Counterfactual Pack Summary

## Grouping
- same_initial_signature: canonical_signature_from_record(record['state_t'])
- group_key_fields: ['state_signature', 'horizon', 'state_step']

## Inputs
- source_data_dir: ..\data_world_model
- source_splits: ['train', 'val', 'test']
- horizons: ['20', 'terminal']
- state_step: 0

## Group Counts
- total_candidate_groups: 6
- eligible_same_initial_groups: 0
- train_visible_groups: 0
- heldout_groups: 0

## Split Coverage
- train_visible_records: 0
- heldout_records: 0
- train_visible_tactic_combos: 0
- heldout_tactic_combos: 0

## Boundary
- train_visible_cf.jsonl is allowed for training/counterfactual finetune.
- heldout_cf.jsonl is eval-only and must not be used in training/finetune.
