# Real vs Predicted Alignment Summary

- episode_id: sim_task_00001
- real_frames: 497
- pred_frames: 81
- matched_events: 0
- likely_events: 5

## Trajectory
- mean_lon_abs_err_deg: 7.047317
- mean_lat_abs_err_deg: 1.072597
- mean_alt_abs_err_m: 1810.116
- mean_speed_abs_err_mps: 206.495
- mean_heading_abs_err_deg: 53.729
- alive_match_ratio: 1.000

## Terminal
- red_win_match: False
- termination_reason_match: False
- red_alive_delta: 0
- blue_alive_delta: 0
- red_missile_delta: -1
- blue_missile_delta: 1

## Notes
- Trajectory drift is dominated by altitude deviation.
- Horizontal position drift is significant.
- Some predicted events are low-confidence Likely markers rather than high-confidence hits.
- Terminal winner prediction mismatches real rollout.
