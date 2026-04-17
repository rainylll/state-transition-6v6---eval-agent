# Held-out Counterfactual Validation Report

## Verdict
- valid: True
- coverage_insufficient: False
- min_groups: 5
- min_pairs: 20
- heldout_groups: 16
- heldout_pairs: 160

## Grouping
- canonical_grouping_unified: True
- same_initial_signature: canonical_state_signature(state_t)
- group_key_fields: ['state_signature', 'horizon', 'state_step']

## Train-visible vs Held-out
- train_visible_records: 160
- train_visible_groups: 32
- train_visible_pairs: 320
- heldout_records: 80
- heldout_groups: 16
- heldout_pairs: 160

## Focus Metrics (Held-out)
- event_red_fire_count: delta_mae=1.025002042390406 | signal_pairs=61 | sign_acc=0.4426229508196721
- termination_flag: delta_mae=3.509979286386855e-10 | signal_pairs=0 | sign_acc=None
- reward_red: delta_mae=0.5236555165261961 | signal_pairs=60 | sign_acc=0.6

## Evaluation Semantics
- reward metrics: denormalized raw reward delta
- termination metric: binary termination flag delta
- termination reason: not part of counterfactual delta metric

## Conclusion
- Held-out counterfactual result is VALID for this run.
