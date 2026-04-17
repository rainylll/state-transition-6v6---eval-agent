# Real vs Predicted Alignment Summary

- episode_id: sim_task_00002
- real_frames: 386
- pred_frames: 81
- matched_events: 0
- likely_events: 1

## Trajectory
- mean_lon_abs_err_deg: 7.368440
- mean_lat_abs_err_deg: 3.735847
- mean_alt_abs_err_m: 903.734
- mean_speed_abs_err_mps: 263.111
- mean_heading_abs_err_deg: 57.731
- alive_match_ratio: 1.000

## Terminal
- red_win_match: True
- termination_reason_match: False
- red_alive_delta: 0
- blue_alive_delta: 1
- red_missile_delta: 4
- blue_missile_delta: 1

## Notes
- Trajectory drift is dominated by altitude deviation.
- Horizontal position drift is significant.
- Some predicted events are low-confidence Likely markers rather than high-confidence hits.
