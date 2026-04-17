# Real vs Predicted Alignment Summary

- episode_id: sim_task_00003
- real_frames: 475
- pred_frames: 81
- matched_events: 0
- likely_events: 5

## Trajectory
- mean_lon_abs_err_deg: 4.325161
- mean_lat_abs_err_deg: 2.344377
- mean_alt_abs_err_m: 649.179
- mean_speed_abs_err_mps: 207.554
- mean_heading_abs_err_deg: 96.732
- alive_match_ratio: 1.000

## Terminal
- red_win_match: True
- termination_reason_match: False
- red_alive_delta: 0
- blue_alive_delta: 1
- red_missile_delta: 3
- blue_missile_delta: -2

## Notes
- Trajectory drift is dominated by altitude deviation.
- Horizontal position drift is significant.
- Some predicted events are low-confidence Likely markers rather than high-confidence hits.
