# Held-out Counterfactual Validation Report

## Verdict
- valid: False
- coverage_insufficient: True
- min_groups: 3
- min_pairs: 10
- heldout_groups: 0
- heldout_pairs: 0

## Grouping
- canonical_grouping_unified: True
- same_initial_signature: canonical_state_signature(state_t)
- group_key_fields: ['state_signature', 'horizon', 'state_step']

## Train-visible vs Held-out
- train_visible_records: 0
- train_visible_groups: 0
- train_visible_pairs: 0
- heldout_records: 0
- heldout_groups: 0
- heldout_pairs: 0

## Focus Metrics (Held-out)
- event_red_fire_count: delta_mae=0.0 | signal_pairs=0 | sign_acc=None
- termination_flag: delta_mae=0.0 | signal_pairs=0 | sign_acc=None
- reward_red: delta_mae=0.0 | signal_pairs=0 | sign_acc=None

## Evaluation Semantics
- reward metrics: denormalized raw reward delta
- termination metric: binary termination flag delta
- termination reason: not part of counterfactual delta metric

## Conclusion
- Held-out counterfactual result is INVALID for this run due to insufficient coverage.
