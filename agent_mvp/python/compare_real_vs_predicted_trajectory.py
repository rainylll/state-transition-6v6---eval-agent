import argparse
import math
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import torch

from data_io import write_json
from predicted_trajectory_to_acmi import (
    load_seed_record,
    rollout_predicted_trajectory,
)
from trajectory_to_acmi import load_rollout_rows, rollout_rows_to_trajectory
from world_model import WorldModelNet


def _to_float(value: object, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _to_int(value: object, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _extract_tactic_id(condition: Dict) -> Optional[int]:
    if not isinstance(condition, dict):
        return None
    params = condition.get("params", {})
    if isinstance(params, dict) and "tactic_id" in params:
        return _to_int(params.get("tactic_id"), 0)
    if "tactic_id" in condition:
        return _to_int(condition.get("tactic_id"), 0)
    if "id" in condition:
        raw = str(condition.get("id", "")).strip()
        digits = "".join(ch for ch in raw if ch.isdigit() or ch == "-")
        if digits and digits != "-":
            return _to_int(digits, 0)
    return None


def _state_signature(state: Dict) -> List[Tuple[str, str, float, float, float, float, float, int, int]]:
    signature: List[Tuple[str, str, float, float, float, float, float, int, int]] = []
    for side in ("red", "blue"):
        for unit in state.get(f"{side}_units", []):
            signature.append(
                (
                    side,
                    str(unit.get("unit_id", "")),
                    round(_to_float(unit.get("lon", 0.0)), 6),
                    round(_to_float(unit.get("lat", 0.0)), 6),
                    round(_to_float(unit.get("alt_m", 0.0)), 3),
                    round(_to_float(unit.get("speed_mps", 0.0)), 3),
                    round(_to_float(unit.get("heading_deg", 0.0)), 3),
                    _to_int(unit.get("missile_count", 0), 0),
                    _to_int(unit.get("alive", 0), 0),
                )
            )
    return sorted(signature)


def _resolve_seed_side_condition(seed_record: Dict, side: str) -> Dict:
    direct = seed_record.get(f"{side}_tactic_condition", {})
    if isinstance(direct, dict) and direct:
        return dict(direct)

    shared = seed_record.get("tactic_condition", {})
    if isinstance(shared, dict) and shared:
        out = dict(shared)
        out["side"] = side
        return out

    shared_id = seed_record.get("tactic_id", None)
    if shared_id is not None:
        return {"id": shared_id, "side": side, "source": "shared_tactic_id"}

    return {}


def _heading_abs_error_deg(left_deg: float, right_deg: float) -> float:
    diff = abs(float(left_deg) - float(right_deg))
    return min(diff, 360.0 - diff)


def _extract_state_from_frame(frame) -> Dict[str, Dict]:
    state: Dict[str, Dict] = {}
    for object_key, obj in frame.objects.items():
        state[object_key] = {
            "lon": float(obj.lon),
            "lat": float(obj.lat),
            "alt_m": float(obj.alt_m),
            "speed_mps": float(obj.speed_mps),
            "heading_deg": float(obj.heading_deg),
            "alive": bool(obj.alive),
            "missile_count": int(obj.missile_count),
            "side": str(obj.side),
        }
    return state


def _nearest_real_frame_index(real_times: List[float], pred_t: float) -> int:
    best_index = 0
    best_error = float("inf")
    for index, value in enumerate(real_times):
        err = abs(value - pred_t)
        if err < best_error:
            best_error = err
            best_index = index
    return best_index


def _mean(values: List[float]) -> float:
    if not values:
        return 0.0
    return float(sum(values) / len(values))


def _max(values: List[float]) -> float:
    if not values:
        return 0.0
    return float(max(values))


def _extract_real_event_times(rows: List[Dict]) -> Dict[str, Optional[float]]:
    event_key_map = {
        "red_first_contact": "red_first_contact_flag",
        "blue_first_contact": "blue_first_contact_flag",
        "red_first_fire": "red_first_fire_flag",
        "blue_first_fire": "blue_first_fire_flag",
        "red_retarget": "red_retarget_flag",
        "blue_retarget": "blue_retarget_flag",
        "red_first_kill": "red_first_kill_flag",
        "blue_first_kill": "blue_first_kill_flag",
        "red_objective_complete": "red_objective_complete_flag",
        "blue_objective_complete": "blue_objective_complete_flag",
        "termination": "termination_flag",
    }
    result: Dict[str, Optional[float]] = {key: None for key in event_key_map}
    for row in rows:
        event = row.get("event", {}) or {}
        sim_time_s = _to_float(row.get("sim_time_s", 0.0))
        for event_name, payload_key in event_key_map.items():
            if result[event_name] is not None:
                continue
            if bool(event.get(payload_key, False)):
                result[event_name] = sim_time_s
        if result["termination"] is None and bool(row.get("done", False)):
            result["termination"] = sim_time_s
    return result


def _extract_pred_event_times(
    step_signals: List[Dict],
    high_prob_threshold: float,
    likely_prob_threshold: float,
) -> Dict[str, Dict[str, Optional[float]]]:
    event_key_map = {
        "red_first_contact": "red_first_contact_flag",
        "blue_first_contact": "blue_first_contact_flag",
        "red_first_fire": "red_first_fire_flag",
        "blue_first_fire": "blue_first_fire_flag",
        "red_retarget": "red_retarget_flag",
        "blue_retarget": "blue_retarget_flag",
        "red_first_kill": "red_first_kill_flag",
        "blue_first_kill": "blue_first_kill_flag",
        "red_objective_complete": "red_objective_complete_flag",
        "blue_objective_complete": "blue_objective_complete_flag",
        "termination": "termination_flag",
    }
    result: Dict[str, Dict[str, Optional[float]]] = {}
    for event_name, payload_key in event_key_map.items():
        best_prob = -1.0
        best_step = None
        first_high_step = None
        for step_idx, step in enumerate(step_signals, start=1):
            prob = _to_float((step.get("event_probs", {}) or {}).get(payload_key, 0.0))
            if prob > best_prob:
                best_prob = prob
                best_step = step_idx
            if first_high_step is None and prob >= high_prob_threshold:
                first_high_step = step_idx

        if first_high_step is not None:
            result[event_name] = {
                "time_s": float(first_high_step),
                "is_likely": False,
                "prob": best_prob,
            }
            continue

        if best_step is not None and best_prob >= likely_prob_threshold:
            result[event_name] = {
                "time_s": float(best_step),
                "is_likely": True,
                "prob": best_prob,
            }
            continue

        result[event_name] = {
            "time_s": None,
            "is_likely": None,
            "prob": best_prob if best_prob >= 0.0 else None,
        }
    return result


def _compare_event_times(
    real_event_times: Dict[str, Optional[float]],
    pred_event_times: Dict[str, Dict[str, Optional[float]]],
) -> Dict[str, Dict]:
    report: Dict[str, Dict] = {}
    for event_name, real_time in real_event_times.items():
        pred_info = pred_event_times.get(event_name, {"time_s": None, "is_likely": None, "prob": None})
        pred_time = pred_info.get("time_s")
        status = "matched"
        delta = None
        if real_time is None and pred_time is None:
            status = "both_missing"
        elif real_time is None and pred_time is not None:
            status = "missing_in_real"
        elif real_time is not None and pred_time is None:
            status = "missing_in_pred"
        else:
            delta = float(pred_time - real_time)

        report[event_name] = {
            "status": status,
            "real_time_s": real_time,
            "pred_time_s": pred_time,
            "time_delta_s": delta,
            "pred_is_likely": pred_info.get("is_likely"),
            "pred_prob": pred_info.get("prob"),
        }
    return report


def _count_alive_and_missiles(state: Dict) -> Dict[str, int]:
    red_alive = 0
    blue_alive = 0
    red_missile = 0
    blue_missile = 0
    for obj in state.values():
        side = obj["side"]
        alive = bool(obj["alive"])
        missile = _to_int(obj["missile_count"], 0)
        if side == "red":
            red_alive += 1 if alive else 0
            red_missile += missile
        elif side == "blue":
            blue_alive += 1 if alive else 0
            blue_missile += missile
    return {
        "red_alive_final": red_alive,
        "blue_alive_final": blue_alive,
        "red_missile_final": red_missile,
        "blue_missile_final": blue_missile,
    }


def _build_alignment_report(
    episode_id: str,
    real_rows: List[Dict],
    real_episode,
    pred_states: List[Dict],
    step_signals: List[Dict],
    horizon: int,
    event_high_prob_threshold: float,
    event_likely_prob_threshold: float,
    input_pairing: Dict,
) -> Dict:
    real_frames = real_episode.frames
    real_times = [float(frame.sim_time_s) for frame in real_frames]

    real_states = [_extract_state_from_frame(frame) for frame in real_frames]
    pred_frame_times = [float(index) for index in range(len(pred_states))]
    pred_states_by_id: List[Dict[str, Dict]] = []
    for state in pred_states:
        item: Dict[str, Dict] = {}
        for side in ("red", "blue"):
            for unit in state.get(f"{side}_units", []):
                unit_id = str(unit.get("unit_id", ""))
                if not unit_id:
                    continue
                item[unit_id] = {
                    "lon": _to_float(unit.get("lon", 0.0)),
                    "lat": _to_float(unit.get("lat", 0.0)),
                    "alt_m": _to_float(unit.get("alt_m", 0.0)),
                    "speed_mps": _to_float(unit.get("speed_mps", 0.0)),
                    "heading_deg": _to_float(unit.get("heading_deg", 0.0)),
                    "alive": bool(unit.get("alive", 0)),
                    "missile_count": _to_int(unit.get("missile_count", 0), 0),
                    "side": side,
                }
        pred_states_by_id.append(item)

    lon_err: List[float] = []
    lat_err: List[float] = []
    alt_err: List[float] = []
    speed_err: List[float] = []
    heading_err: List[float] = []
    missile_err: List[float] = []
    alive_match: List[float] = []
    frame_reports: List[Dict] = []

    for pred_index, pred_t in enumerate(pred_frame_times):
        real_index = _nearest_real_frame_index(real_times, pred_t)
        pred_state = pred_states_by_id[pred_index]
        real_state = real_states[real_index]
        shared_ids = sorted(set(pred_state.keys()) & set(real_state.keys()))
        if not shared_ids:
            frame_reports.append(
                {
                    "pred_time_s": pred_t,
                    "real_time_s": real_times[real_index],
                    "matched_unit_count": 0,
                }
            )
            continue

        frame_lon: List[float] = []
        frame_lat: List[float] = []
        frame_alt: List[float] = []
        frame_speed: List[float] = []
        frame_heading: List[float] = []
        frame_missile: List[float] = []
        frame_alive: List[float] = []

        for unit_id in shared_ids:
            pred = pred_state[unit_id]
            real = real_state[unit_id]
            d_lon = abs(pred["lon"] - real["lon"])
            d_lat = abs(pred["lat"] - real["lat"])
            d_alt = abs(pred["alt_m"] - real["alt_m"])
            d_speed = abs(pred["speed_mps"] - real["speed_mps"])
            d_heading = _heading_abs_error_deg(pred["heading_deg"], real["heading_deg"])
            d_missile = abs(pred["missile_count"] - real["missile_count"])
            same_alive = 1.0 if bool(pred["alive"]) == bool(real["alive"]) else 0.0

            lon_err.append(d_lon)
            lat_err.append(d_lat)
            alt_err.append(d_alt)
            speed_err.append(d_speed)
            heading_err.append(d_heading)
            missile_err.append(d_missile)
            alive_match.append(same_alive)

            frame_lon.append(d_lon)
            frame_lat.append(d_lat)
            frame_alt.append(d_alt)
            frame_speed.append(d_speed)
            frame_heading.append(d_heading)
            frame_missile.append(d_missile)
            frame_alive.append(same_alive)

        frame_reports.append(
            {
                "pred_time_s": pred_t,
                "real_time_s": real_times[real_index],
                "matched_unit_count": len(shared_ids),
                "mean_lon_abs_err_deg": _mean(frame_lon),
                "mean_lat_abs_err_deg": _mean(frame_lat),
                "mean_alt_abs_err_m": _mean(frame_alt),
                "mean_speed_abs_err_mps": _mean(frame_speed),
                "mean_heading_abs_err_deg": _mean(frame_heading),
                "mean_missile_abs_err": _mean(frame_missile),
                "alive_match_ratio": _mean(frame_alive),
            }
        )

    real_events = _extract_real_event_times(real_rows)
    pred_events = _extract_pred_event_times(
        step_signals=step_signals,
        high_prob_threshold=event_high_prob_threshold,
        likely_prob_threshold=event_likely_prob_threshold,
    )
    event_alignment = _compare_event_times(real_events, pred_events)

    real_terminal = dict(real_rows[-1].get("episode_outcome", {})) if real_rows else {}
    pred_terminal_state = pred_states_by_id[-1] if pred_states_by_id else {}
    pred_terminal_stats = _count_alive_and_missiles(pred_terminal_state)
    pred_red_win_prob = _to_float(step_signals[-1].get("red_win_prob", 0.5), 0.5) if step_signals else 0.5
    pred_red_win = 1 if pred_red_win_prob >= 0.5 else 0
    pred_termination_reason = "predicted_termination_flag" if any(bool(item.get("termination_likely", False)) for item in step_signals) else "predicted_rollout_horizon"

    trajectory_metrics = {
        "mean_lon_abs_err_deg": _mean(lon_err),
        "max_lon_abs_err_deg": _max(lon_err),
        "mean_lat_abs_err_deg": _mean(lat_err),
        "max_lat_abs_err_deg": _max(lat_err),
        "mean_alt_abs_err_m": _mean(alt_err),
        "max_alt_abs_err_m": _max(alt_err),
        "mean_speed_abs_err_mps": _mean(speed_err),
        "max_speed_abs_err_mps": _max(speed_err),
        "mean_heading_abs_err_deg": _mean(heading_err),
        "max_heading_abs_err_deg": _max(heading_err),
        "mean_missile_abs_err": _mean(missile_err),
        "max_missile_abs_err": _max(missile_err),
        "alive_match_ratio": _mean(alive_match),
        "num_aligned_unit_samples": len(alive_match),
        "per_frame": frame_reports,
    }

    terminal_alignment = {
        "real": {
            "red_win": real_terminal.get("red_win"),
            "termination_reason": real_terminal.get("termination_reason"),
            "red_alive_final": real_terminal.get("red_alive_final"),
            "blue_alive_final": real_terminal.get("blue_alive_final"),
            "red_missile_final": real_terminal.get("red_missile_final"),
            "blue_missile_final": real_terminal.get("blue_missile_final"),
        },
        "pred": {
            "red_win_prob": pred_red_win_prob,
            "red_win": pred_red_win,
            "termination_reason": pred_termination_reason,
            **pred_terminal_stats,
        },
        "comparison": {
            "red_win_match": (real_terminal.get("red_win") == pred_red_win) if real_terminal else None,
            "termination_reason_match": (real_terminal.get("termination_reason") == pred_termination_reason) if real_terminal else None,
            "red_alive_delta": _to_int(pred_terminal_stats.get("red_alive_final", 0)) - _to_int(real_terminal.get("red_alive_final", 0)),
            "blue_alive_delta": _to_int(pred_terminal_stats.get("blue_alive_final", 0)) - _to_int(real_terminal.get("blue_alive_final", 0)),
            "red_missile_delta": _to_int(pred_terminal_stats.get("red_missile_final", 0)) - _to_int(real_terminal.get("red_missile_final", 0)),
            "blue_missile_delta": _to_int(pred_terminal_stats.get("blue_missile_final", 0)) - _to_int(real_terminal.get("blue_missile_final", 0)),
        },
    }

    matched_events = sum(1 for item in event_alignment.values() if item["status"] == "matched")
    missing_in_pred = [name for name, item in event_alignment.items() if item["status"] == "missing_in_pred"]
    likely_events = [name for name, item in event_alignment.items() if bool(item.get("pred_is_likely", False))]

    alignment_summary = {
        "episode_id": episode_id,
        "horizon": horizon,
        "real_frame_count": len(real_frames),
        "pred_frame_count": len(pred_states_by_id),
        "matched_event_count": matched_events,
        "missing_event_in_pred": missing_in_pred,
        "likely_event_count": len(likely_events),
    }

    notes: List[str] = []
    if trajectory_metrics["mean_alt_abs_err_m"] > 500.0:
        notes.append("Trajectory drift is dominated by altitude deviation.")
    if trajectory_metrics["mean_lat_abs_err_deg"] > 0.05 or trajectory_metrics["mean_lon_abs_err_deg"] > 0.05:
        notes.append("Horizontal position drift is significant.")
    if len(likely_events) > 0:
        notes.append("Some predicted events are low-confidence Likely markers rather than high-confidence hits.")
    if terminal_alignment["comparison"]["red_win_match"] is False:
        notes.append("Terminal winner prediction mismatches real rollout.")

    return {
        "episode_id": episode_id,
        "input_pairing": input_pairing,
        "alignment_summary": alignment_summary,
        "trajectory_metrics": trajectory_metrics,
        "event_alignment": event_alignment,
        "terminal_alignment": terminal_alignment,
        "notes": notes,
    }


def _render_markdown_summary(report: Dict) -> str:
    summary = report["alignment_summary"]
    traj = report["trajectory_metrics"]
    terminal = report["terminal_alignment"]["comparison"]
    lines: List[str] = []
    lines.append("# Real vs Predicted Alignment Summary")
    lines.append("")
    lines.append(f"- episode_id: {report['episode_id']}")
    lines.append(f"- real_frames: {summary['real_frame_count']}")
    lines.append(f"- pred_frames: {summary['pred_frame_count']}")
    lines.append(f"- matched_events: {summary['matched_event_count']}")
    lines.append(f"- likely_events: {summary['likely_event_count']}")
    lines.append("")
    lines.append("## Trajectory")
    lines.append(f"- mean_lon_abs_err_deg: {traj['mean_lon_abs_err_deg']:.6f}")
    lines.append(f"- mean_lat_abs_err_deg: {traj['mean_lat_abs_err_deg']:.6f}")
    lines.append(f"- mean_alt_abs_err_m: {traj['mean_alt_abs_err_m']:.3f}")
    lines.append(f"- mean_speed_abs_err_mps: {traj['mean_speed_abs_err_mps']:.3f}")
    lines.append(f"- mean_heading_abs_err_deg: {traj['mean_heading_abs_err_deg']:.3f}")
    lines.append(f"- alive_match_ratio: {traj['alive_match_ratio']:.3f}")
    lines.append("")
    lines.append("## Terminal")
    lines.append(f"- red_win_match: {terminal['red_win_match']}")
    lines.append(f"- termination_reason_match: {terminal['termination_reason_match']}")
    lines.append(f"- red_alive_delta: {terminal['red_alive_delta']}")
    lines.append(f"- blue_alive_delta: {terminal['blue_alive_delta']}")
    lines.append(f"- red_missile_delta: {terminal['red_missile_delta']}")
    lines.append(f"- blue_missile_delta: {terminal['blue_missile_delta']}")
    lines.append("")
    lines.append("## Notes")
    if report.get("notes"):
        for item in report["notes"]:
            lines.append(f"- {item}")
    else:
        lines.append("- No additional notes.")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Compare real rollout and world-model predicted rollout under same input.")
    parser.add_argument("--real-rollouts", type=Path, required=True, help="Path to real rollouts.jsonl")
    parser.add_argument("--processed-input", type=Path, required=True, help="Path to processed world-model JSONL")
    parser.add_argument("--model-path", type=Path, required=True, help="Path to trained world-model checkpoint")
    parser.add_argument("--episode-id", type=str, required=True)
    parser.add_argument("--state-step", type=int, default=0)
    parser.add_argument("--horizon", type=int, default=1, choices=[1, 5, 10, 20])
    parser.add_argument("--rollout-steps", type=int, default=80)
    parser.add_argument("--alive-threshold", type=float, default=0.5)
    parser.add_argument("--event-high-prob-threshold", type=float, default=0.6)
    parser.add_argument("--event-likely-prob-threshold", type=float, default=0.001)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument("--out-json", type=Path, required=True)
    parser.add_argument("--out-md", type=Path, default=None)
    args = parser.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    real_rows = load_rollout_rows(args.real_rollouts, args.episode_id)
    real_episode = rollout_rows_to_trajectory(real_rows)

    seed_record = load_seed_record(
        processed_path=args.processed_input,
        episode_id=args.episode_id,
        state_step=args.state_step,
        horizon=args.horizon,
    )

    model = WorldModelNet().to(device)
    model.load_state_dict(torch.load(args.model_path, map_location=device))
    model.eval()

    pred_states, step_signals = rollout_predicted_trajectory(
        model=model,
        initial_state=dict(seed_record.get("state_t", {})),
        red_tactic_condition=dict(seed_record.get("red_tactic_condition", {})),
        blue_tactic_condition=dict(seed_record.get("blue_tactic_condition", {})),
        horizon=args.horizon,
        rollout_steps=args.rollout_steps,
        dt_s=1.0,
        alive_threshold=args.alive_threshold,
        event_prob_threshold=args.event_high_prob_threshold,
        device=device,
    )

    real_initial_state = dict(real_rows[0].get("state", {})) if real_rows else {}
    seed_initial_state = dict(seed_record.get("state_t", {}))
    real_red_tactic = dict(real_rows[0].get("red_tactic_condition", {})) if real_rows else {}
    real_blue_tactic = dict(real_rows[0].get("blue_tactic_condition", {})) if real_rows else {}
    seed_red_tactic = _resolve_seed_side_condition(seed_record, "red")
    seed_blue_tactic = _resolve_seed_side_condition(seed_record, "blue")

    input_pairing = {
        "episode_id_match": bool(real_rows) and str(real_rows[0].get("episode_id", "")) == args.episode_id,
        "state_step": args.state_step,
        "horizon": args.horizon,
        "initial_state_exact_match": _state_signature(real_initial_state) == _state_signature(seed_initial_state),
        "red_tactic_id_real": _extract_tactic_id(real_red_tactic),
        "red_tactic_id_seed": _extract_tactic_id(seed_red_tactic),
        "blue_tactic_id_real": _extract_tactic_id(real_blue_tactic),
        "blue_tactic_id_seed": _extract_tactic_id(seed_blue_tactic),
        "red_tactic_id_match": _extract_tactic_id(real_red_tactic) == _extract_tactic_id(seed_red_tactic),
        "blue_tactic_id_match": _extract_tactic_id(real_blue_tactic) == _extract_tactic_id(seed_blue_tactic),
    }

    report = _build_alignment_report(
        episode_id=args.episode_id,
        real_rows=real_rows,
        real_episode=real_episode,
        pred_states=pred_states,
        step_signals=step_signals,
        horizon=args.horizon,
        event_high_prob_threshold=args.event_high_prob_threshold,
        event_likely_prob_threshold=args.event_likely_prob_threshold,
        input_pairing=input_pairing,
    )

    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.out_json, report)

    if args.out_md is not None:
        args.out_md.parent.mkdir(parents=True, exist_ok=True)
        content = _render_markdown_summary(report)
        args.out_md.write_text(content, encoding="utf-8")

    print(f"Alignment JSON written to: {args.out_json}")
    if args.out_md is not None:
        print(f"Alignment markdown written to: {args.out_md}")
    print(f"Episode: {args.episode_id}")
    print(f"Mean lat err(deg): {report['trajectory_metrics']['mean_lat_abs_err_deg']:.6f}")
    print(f"Mean lon err(deg): {report['trajectory_metrics']['mean_lon_abs_err_deg']:.6f}")
    print(f"Mean alt err(m): {report['trajectory_metrics']['mean_alt_abs_err_m']:.3f}")


if __name__ == "__main__":
    main()
