# Clean Case Benchmark Summary

- requested_cases: 30
- selected_clean_cases: 30
- horizons: [1, 5, 10, 20]

## Failure Thresholds
- mean_lon_abs_err_deg: 5.0
- mean_lat_abs_err_deg: 2.0
- mean_alt_abs_err_m: 1000.0
- mean_speed_abs_err_mps: 220.0
- mean_heading_abs_err_deg: 70.0
- mean_missile_abs_err: 1.0
- alive_match_ratio: 0.95

## Horizon 1
- num_cases: 30
- trajectory:
  - mean_lon_abs_err_deg: mean=4.7687, p50=4.1424, p90=8.7700, max=12.9064, failure_rate=0.300
  - mean_lat_abs_err_deg: mean=2.2992, p50=2.3758, p90=3.6676, max=3.9340, failure_rate=0.600
  - mean_alt_abs_err_m: mean=1848.1886, p50=1815.1699, p90=2750.4804, max=3124.0290, failure_rate=0.900
  - mean_speed_abs_err_mps: mean=193.1487, p50=214.4668, p90=238.9176, max=270.1928, failure_rate=0.433
  - mean_heading_abs_err_deg: mean=59.2322, p50=57.4045, p90=87.4176, max=119.1321, failure_rate=0.267
  - mean_missile_abs_err: mean=1.6593, p50=2.0792, p90=3.0186, max=4.7006, failure_rate=0.567
  - alive_match_ratio: mean=1.0000, p50=1.0000, p90=1.0000, max=1.0000, failure_rate=0.000
- events: high_conf_matched=0, likely_matched=0, missing_in_pred=30, missing_in_real=194, likely_count=194, term_hit_any=0.000, term_hit_high_conf=0.000, term_hit_likely=0.000
- terminal: red_win_match_rate=0.333, termination_reason_match_rate=0.000

## Horizon 5
- num_cases: 30
- trajectory:
  - mean_lon_abs_err_deg: mean=5.3474, p50=5.5039, p90=7.1301, max=10.2832, failure_rate=0.667
  - mean_lat_abs_err_deg: mean=1.9454, p50=1.7629, p90=3.0941, max=3.3290, failure_rate=0.467
  - mean_alt_abs_err_m: mean=1942.7590, p50=1912.6692, p90=2507.8517, max=3150.9125, failure_rate=0.933
  - mean_speed_abs_err_mps: mean=150.5620, p50=150.7149, p90=214.2946, max=220.1022, failure_rate=0.033
  - mean_heading_abs_err_deg: mean=55.6188, p50=57.4410, p90=84.1541, max=115.9383, failure_rate=0.267
  - mean_missile_abs_err: mean=1.6028, p50=1.3204, p90=3.0828, max=4.5535, failure_rate=0.667
  - alive_match_ratio: mean=1.0000, p50=1.0000, p90=1.0000, max=1.0000, failure_rate=0.000
- events: high_conf_matched=0, likely_matched=0, missing_in_pred=30, missing_in_real=213, likely_count=213, term_hit_any=0.000, term_hit_high_conf=0.000, term_hit_likely=0.000
- terminal: red_win_match_rate=0.467, termination_reason_match_rate=0.000

## Horizon 10
- num_cases: 30
- trajectory:
  - mean_lon_abs_err_deg: mean=5.2557, p50=4.7269, p90=8.4809, max=9.8378, failure_rate=0.467
  - mean_lat_abs_err_deg: mean=2.4784, p50=2.4771, p90=3.8586, max=4.0951, failure_rate=0.733
  - mean_alt_abs_err_m: mean=1951.7626, p50=1993.0290, p90=2904.8108, max=3410.8863, failure_rate=0.967
  - mean_speed_abs_err_mps: mean=134.7084, p50=133.9645, p90=208.7156, max=209.7740, failure_rate=0.000
  - mean_heading_abs_err_deg: mean=60.8930, p50=58.2265, p90=88.4812, max=118.9286, failure_rate=0.333
  - mean_missile_abs_err: mean=1.9010, p50=1.9959, p90=3.5303, max=4.8056, failure_rate=0.633
  - alive_match_ratio: mean=1.0000, p50=1.0000, p90=1.0000, max=1.0000, failure_rate=0.000
- events: high_conf_matched=0, likely_matched=6, missing_in_pred=24, missing_in_real=230, likely_count=236, term_hit_any=0.200, term_hit_high_conf=0.000, term_hit_likely=0.200
- terminal: red_win_match_rate=0.367, termination_reason_match_rate=0.000

## Horizon 20
- num_cases: 30
- trajectory:
  - mean_lon_abs_err_deg: mean=4.5665, p50=4.2821, p90=5.8593, max=7.1022, failure_rate=0.367
  - mean_lat_abs_err_deg: mean=2.4280, p50=2.4083, p90=4.4611, max=4.7612, failure_rate=0.667
  - mean_alt_abs_err_m: mean=1941.5702, p50=1951.0434, p90=2780.1762, max=3372.0662, failure_rate=0.900
  - mean_speed_abs_err_mps: mean=127.4724, p50=116.6059, p90=194.1769, max=195.2214, failure_rate=0.000
  - mean_heading_abs_err_deg: mean=60.1967, p50=55.4646, p90=89.2425, max=119.0551, failure_rate=0.333
  - mean_missile_abs_err: mean=2.1116, p50=2.0008, p90=3.6509, max=4.3395, failure_rate=0.767
  - alive_match_ratio: mean=1.0000, p50=1.0000, p90=1.0000, max=1.0000, failure_rate=0.000
- events: high_conf_matched=0, likely_matched=6, missing_in_pred=24, missing_in_real=236, likely_count=242, term_hit_any=0.200, term_hit_high_conf=0.000, term_hit_likely=0.200
- terminal: red_win_match_rate=0.333, termination_reason_match_rate=0.000

## Best Cases
- rollouts_legacy_only.jsonl::legacy_task_00001 score=5.3600
- rollouts_dual_tactics.jsonl::sim_task_00003 score=5.8251
- rollouts_phase35_mix16.jsonl::sim_task_00002 score=5.8575
- rollouts_phase3_mix_runner8.jsonl::sim_task_00002 score=5.8575
- rollouts_phase35_mix16.jsonl::sim_task_00003 score=6.4421

## Worst Cases
- rollouts_phase35_mix16.jsonl::sim_task_00004 score=9.0933
- rollouts_phase3_mix_runner8.jsonl::sim_task_00004 score=9.0933
- rollouts_phase35_mix16.jsonl::sim_task_00008 score=9.2753
- rollouts_phase3_mix_runner8.jsonl::sim_task_00008 score=9.2753
- rollouts_phase35_mix16.jsonl::sim_task_00015 score=9.2903

## Engineering Judgment
- Trajectory error generally increases at longer horizon, consistent with rollout drift accumulation.
- Event-layer high-confidence matches are sparse; likely events are more frequent than high-confidence hits.
- Terminal winner may partially match, but termination_reason consistency remains a major weakness.
