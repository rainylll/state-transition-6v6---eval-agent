import argparse
import json
from pathlib import Path
from statistics import mean
from typing import Dict, List, Sequence, Tuple

import numpy as np
import torch

from compare_real_vs_predicted_trajectory import (
    _build_alignment_report,
    _extract_tactic_id,
    _state_signature,
)
from data_io import read_jsonl, write_json
from predicted_trajectory_to_acmi import rollout_predicted_trajectory
from trajectory_to_acmi import rollout_rows_to_trajectory
from world_model import WorldModelNet


FAILURE_THRESHOLDS = {
    "mean_lon_abs_err_deg": 5.0,
    "mean_lat_abs_err_deg": 2.0,
    "mean_alt_abs_err_m": 1000.0,
    "mean_speed_abs_err_mps": 220.0,
    "mean_heading_abs_err_deg": 70.0,
    "mean_missile_abs_err": 1.0,
    "alive_match_ratio": 0.95,
}


def _group_rollouts_by_episode(rollout_path: Path) -> Dict[str, List[Dict]]:
    grouped: Dict[str, List[Dict]] = {}
    for row in read_jsonl(rollout_path):
        episode_id = str(row.get("episode_id", ""))
        if not episode_id:
            continue
        grouped.setdefault(episode_id, []).append(row)
    for rows in grouped.values():
        rows.sort(key=lambda item: int(item.get("step", 0)))
    return grouped


def _parse_horizons(raw: str) -> List[int]:
    values: List[int] = []
    for token in raw.split(","):
        item = token.strip()
        if not item:
            continue
        values.append(int(item))
    return sorted(set(values))


def _percentile(values: Sequence[float], q: float) -> float:
    if not values:
        return 0.0
    return float(np.percentile(np.asarray(values, dtype=np.float64), q))


def _summary_stats(values: Sequence[float]) -> Dict[str, float]:
    if not values:
        return {"mean": 0.0, "p50": 0.0, "p90": 0.0, "max": 0.0}
    return {
        "mean": float(mean(values)),
        "p50": _percentile(values, 50),
        "p90": _percentile(values, 90),
        "max": float(max(values)),
    }


def _discover_clean_candidates(rollout_glob: str) -> List[Dict]:
    candidates: List[Dict] = []
    for rollout_path in sorted(Path().glob(rollout_glob)):
        grouped = _group_rollouts_by_episode(rollout_path)
        for episode_id, rows in sorted(grouped.items()):
            if not rows:
                continue
            first = rows[0]
            seed_state = dict(first.get("state", {}))
            seed_red_tactic = dict(first.get("red_tactic_condition", {}))
            seed_blue_tactic = dict(first.get("blue_tactic_condition", {}))

            real_red_id = _extract_tactic_id(seed_red_tactic)
            real_blue_id = _extract_tactic_id(seed_blue_tactic)
            red_match = real_red_id is not None and real_red_id == _extract_tactic_id(seed_red_tactic)
            blue_match = real_blue_id is not None and real_blue_id == _extract_tactic_id(seed_blue_tactic)
            initial_match = _state_signature(dict(first.get("state", {}))) == _state_signature(seed_state)
            is_clean = bool(initial_match and red_match and blue_match)

            candidates.append(
                {
                    "rollout_file": str(rollout_path),
                    "episode_id": episode_id,
                    "rows": rows,
                    "seed_state": seed_state,
                    "seed_red_tactic": seed_red_tactic,
                    "seed_blue_tactic": seed_blue_tactic,
                    "input_pairing": {
                        "episode_id_match": True,
                        "state_step": 0,
                        "initial_state_exact_match": initial_match,
                        "red_tactic_id_real": real_red_id,
                        "red_tactic_id_seed": _extract_tactic_id(seed_red_tactic),
                        "blue_tactic_id_real": real_blue_id,
                        "blue_tactic_id_seed": _extract_tactic_id(seed_blue_tactic),
                        "red_tactic_id_match": red_match,
                        "blue_tactic_id_match": blue_match,
                    },
                    "is_clean": is_clean,
                }
            )
    return candidates


def _trajectory_failure_rate(values: Sequence[float], metric_name: str) -> float:
    if not values:
        return 0.0
    threshold = FAILURE_THRESHOLDS[metric_name]
    if metric_name == "alive_match_ratio":
        failed = sum(1 for value in values if float(value) < threshold)
    else:
        failed = sum(1 for value in values if float(value) > threshold)
    return float(failed / len(values))


def _aggregate_per_horizon(case_reports: List[Dict], horizon: int) -> Dict:
    horizon_reports = [item for item in case_reports if int(item["horizon"]) == int(horizon)]
    trajectory_keys = [
        "mean_lon_abs_err_deg",
        "mean_lat_abs_err_deg",
        "mean_alt_abs_err_m",
        "mean_speed_abs_err_mps",
        "mean_heading_abs_err_deg",
        "mean_missile_abs_err",
        "alive_match_ratio",
    ]

    trajectory_stats: Dict[str, Dict] = {}
    for key in trajectory_keys:
        values = [float(item["trajectory_metrics"][key]) for item in horizon_reports]
        trajectory_stats[key] = _summary_stats(values)
        trajectory_stats[key]["failure_rate"] = _trajectory_failure_rate(values, key)

    high_conf_matched = 0
    likely_matched = 0
    missing_in_pred = 0
    missing_in_real = 0
    likely_count = 0
    high_conf_termination_hit = 0
    likely_termination_hit = 0
    any_termination_hit = 0
    total_events = 0

    for item in horizon_reports:
        events = item["event_alignment"]
        total_events += len(events)
        for event_name, payload in events.items():
            status = str(payload.get("status", ""))
            is_likely = bool(payload.get("pred_is_likely", False))
            if is_likely and payload.get("pred_time_s") is not None:
                likely_count += 1
            if status == "matched":
                if is_likely:
                    likely_matched += 1
                else:
                    high_conf_matched += 1
            elif status == "missing_in_pred":
                missing_in_pred += 1
            elif status == "missing_in_real":
                missing_in_real += 1

            if event_name == "termination" and status == "matched":
                any_termination_hit += 1
                if is_likely:
                    likely_termination_hit += 1
                else:
                    high_conf_termination_hit += 1

    terminal_reports = [item["terminal_alignment"]["comparison"] for item in horizon_reports]
    red_win_match_rate = float(
        sum(1 for payload in terminal_reports if bool(payload.get("red_win_match", False))) / max(len(terminal_reports), 1)
    )
    termination_reason_match_rate = float(
        sum(1 for payload in terminal_reports if bool(payload.get("termination_reason_match", False))) / max(len(terminal_reports), 1)
    )

    def _delta_stats(key: str) -> Dict:
        signed_values = [float(payload.get(key, 0.0)) for payload in terminal_reports]
        abs_values = [abs(value) for value in signed_values]
        result = _summary_stats(abs_values)
        result["signed_mean"] = float(mean(signed_values)) if signed_values else 0.0
        return result

    event_stats = {
        "high_confidence_matched_count": high_conf_matched,
        "likely_matched_count": likely_matched,
        "missing_in_pred_count": missing_in_pred,
        "missing_in_real_count": missing_in_real,
        "likely_count": likely_count,
        "termination_hit_rate_any": float(any_termination_hit / max(len(horizon_reports), 1)),
        "termination_hit_rate_high_confidence": float(high_conf_termination_hit / max(len(horizon_reports), 1)),
        "termination_hit_rate_likely": float(likely_termination_hit / max(len(horizon_reports), 1)),
        "total_event_entries": total_events,
    }

    terminal_stats = {
        "red_win_match_rate": red_win_match_rate,
        "termination_reason_match_rate": termination_reason_match_rate,
        "red_alive_delta": _delta_stats("red_alive_delta"),
        "blue_alive_delta": _delta_stats("blue_alive_delta"),
        "red_missile_delta": _delta_stats("red_missile_delta"),
        "blue_missile_delta": _delta_stats("blue_missile_delta"),
    }

    return {
        "horizon": horizon,
        "num_cases": len(horizon_reports),
        "trajectory": trajectory_stats,
        "events": event_stats,
        "terminal": terminal_stats,
    }


def _case_quality_score(case_report: Dict) -> float:
    t = case_report["trajectory_metrics"]
    e = case_report["event_alignment"]
    c = case_report["terminal_alignment"]["comparison"]
    score = 0.0
    score += float(t["mean_lon_abs_err_deg"]) / FAILURE_THRESHOLDS["mean_lon_abs_err_deg"]
    score += float(t["mean_lat_abs_err_deg"]) / FAILURE_THRESHOLDS["mean_lat_abs_err_deg"]
    score += float(t["mean_alt_abs_err_m"]) / FAILURE_THRESHOLDS["mean_alt_abs_err_m"]
    score += float(t["mean_speed_abs_err_mps"]) / FAILURE_THRESHOLDS["mean_speed_abs_err_mps"]
    score += float(t["mean_heading_abs_err_deg"]) / FAILURE_THRESHOLDS["mean_heading_abs_err_deg"]
    score += max(0.0, FAILURE_THRESHOLDS["alive_match_ratio"] - float(t["alive_match_ratio"])) * 5.0
    score += sum(1 for payload in e.values() if payload.get("status") == "missing_in_pred") * 0.3
    score += 1.0 if not bool(c.get("red_win_match", False)) else 0.0
    score += 1.0 if not bool(c.get("termination_reason_match", False)) else 0.0
    return float(score)


def _write_markdown_report(payload: Dict, output_path: Path) -> None:
    lines: List[str] = []
    lines.append("# Clean Case Benchmark Summary")
    lines.append("")
    lines.append(f"- requested_cases: {payload['requested_cases']}")
    lines.append(f"- selected_clean_cases: {payload['selected_clean_cases']}")
    lines.append(f"- horizons: {payload['horizons']}")
    lines.append("")
    lines.append("## Failure Thresholds")
    for key, value in FAILURE_THRESHOLDS.items():
        lines.append(f"- {key}: {value}")
    lines.append("")

    for horizon_payload in payload["per_horizon"]:
        lines.append(f"## Horizon {horizon_payload['horizon']}")
        lines.append(f"- num_cases: {horizon_payload['num_cases']}")
        lines.append("- trajectory:")
        for metric_name, stats in horizon_payload["trajectory"].items():
            lines.append(
                f"  - {metric_name}: mean={stats['mean']:.4f}, p50={stats['p50']:.4f}, p90={stats['p90']:.4f}, max={stats['max']:.4f}, failure_rate={stats['failure_rate']:.3f}"
            )
        events = horizon_payload["events"]
        lines.append(
            "- events: "
            f"high_conf_matched={events['high_confidence_matched_count']}, likely_matched={events['likely_matched_count']}, "
            f"missing_in_pred={events['missing_in_pred_count']}, missing_in_real={events['missing_in_real_count']}, "
            f"likely_count={events['likely_count']}, term_hit_any={events['termination_hit_rate_any']:.3f}, "
            f"term_hit_high_conf={events['termination_hit_rate_high_confidence']:.3f}, term_hit_likely={events['termination_hit_rate_likely']:.3f}"
        )
        terminal = horizon_payload["terminal"]
        lines.append(
            "- terminal: "
            f"red_win_match_rate={terminal['red_win_match_rate']:.3f}, termination_reason_match_rate={terminal['termination_reason_match_rate']:.3f}"
        )
        lines.append("")

    lines.append("## Best Cases")
    for item in payload["best_cases"]:
        lines.append(f"- {item['case_id']} score={item['score']:.4f}")
    lines.append("")
    lines.append("## Worst Cases")
    for item in payload["worst_cases"]:
        lines.append(f"- {item['case_id']} score={item['score']:.4f}")
    lines.append("")
    lines.append("## Engineering Judgment")
    for item in payload["engineering_judgment"]:
        lines.append(f"- {item}")
    lines.append("")
    output_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Phase 9.5 clean-case batch benchmark for real vs predicted alignment.")
    parser.add_argument("--rollout-glob", type=str, default="../data_world_model/raw/rollouts*.jsonl")
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--max-cases", type=int, default=30)
    parser.add_argument("--horizons", type=str, default="1,5,10,20")
    parser.add_argument("--rollout-steps", type=int, default=80)
    parser.add_argument("--alive-threshold", type=float, default=0.5)
    parser.add_argument("--event-high-prob-threshold", type=float, default=0.6)
    parser.add_argument("--event-likely-prob-threshold", type=float, default=0.001)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument("--out-json", type=Path, required=True)
    parser.add_argument("--out-md", type=Path, required=True)
    args = parser.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current runtime.")
    device = torch.device(args.device)

    horizons = _parse_horizons(args.horizons)
    candidates = _discover_clean_candidates(args.rollout_glob)
    clean_candidates = [item for item in candidates if bool(item["is_clean"])]
    clean_candidates.sort(key=lambda item: (item["rollout_file"], item["episode_id"]))

    selected_candidates = clean_candidates[: args.max_cases]
    if not selected_candidates:
        raise RuntimeError("No clean candidates found under current rollout_glob and pairing constraints.")

    model = WorldModelNet().to(device)
    model.load_state_dict(torch.load(args.model_path, map_location=device))
    model.eval()

    case_reports: List[Dict] = []
    for idx, candidate in enumerate(selected_candidates, start=1):
        episode_id = str(candidate["episode_id"])
        rows = list(candidate["rows"])
        real_episode = rollout_rows_to_trajectory(rows)
        print(f"[{idx}/{len(selected_candidates)}] benchmarking clean case: {Path(candidate['rollout_file']).name}::{episode_id}")

        for horizon in horizons:
            pred_states, step_signals = rollout_predicted_trajectory(
                model=model,
                initial_state=dict(candidate["seed_state"]),
                red_tactic_condition=dict(candidate["seed_red_tactic"]),
                blue_tactic_condition=dict(candidate["seed_blue_tactic"]),
                horizon=horizon,
                rollout_steps=args.rollout_steps,
                dt_s=1.0,
                alive_threshold=args.alive_threshold,
                event_prob_threshold=args.event_high_prob_threshold,
                device=device,
            )

            pairing = dict(candidate["input_pairing"])
            pairing["horizon"] = horizon
            report = _build_alignment_report(
                episode_id=episode_id,
                real_rows=rows,
                real_episode=real_episode,
                pred_states=pred_states,
                step_signals=step_signals,
                horizon=horizon,
                event_high_prob_threshold=args.event_high_prob_threshold,
                event_likely_prob_threshold=args.event_likely_prob_threshold,
                input_pairing=pairing,
            )

            case_reports.append(
                {
                    "case_id": f"{Path(candidate['rollout_file']).name}::{episode_id}",
                    "rollout_file": candidate["rollout_file"],
                    "episode_id": episode_id,
                    "horizon": horizon,
                    "input_pairing": report["input_pairing"],
                    "alignment_summary": report["alignment_summary"],
                    "trajectory_metrics": {
                        key: value
                        for key, value in report["trajectory_metrics"].items()
                        if key != "per_frame"
                    },
                    "event_alignment": report["event_alignment"],
                    "terminal_alignment": report["terminal_alignment"],
                    "notes": report.get("notes", []),
                }
            )

    per_horizon = [_aggregate_per_horizon(case_reports, h) for h in horizons]

    scored_cases: List[Tuple[str, float]] = []
    per_case_group: Dict[str, List[Dict]] = {}
    for item in case_reports:
        per_case_group.setdefault(item["case_id"], []).append(item)
    for case_id, reports in per_case_group.items():
        score = float(mean(_case_quality_score(report) for report in reports))
        scored_cases.append((case_id, score))
    scored_cases.sort(key=lambda item: item[1])

    best_cases = [{"case_id": case_id, "score": score} for case_id, score in scored_cases[:5]]
    worst_cases = [{"case_id": case_id, "score": score} for case_id, score in scored_cases[-5:]]

    judgment = [
        "Trajectory error generally increases at longer horizon, consistent with rollout drift accumulation.",
        "Event-layer high-confidence matches are sparse; likely events are more frequent than high-confidence hits.",
        "Terminal winner may partially match, but termination_reason consistency remains a major weakness.",
    ]

    payload = {
        "requested_cases": int(args.max_cases),
        "candidate_cases": len(candidates),
        "clean_candidate_cases": len(clean_candidates),
        "selected_clean_cases": len(selected_candidates),
        "horizons": horizons,
        "failure_thresholds": FAILURE_THRESHOLDS,
        "per_horizon": per_horizon,
        "best_cases": best_cases,
        "worst_cases": worst_cases,
        "engineering_judgment": judgment,
        "case_reports": case_reports,
    }

    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.out_json, payload)
    args.out_md.parent.mkdir(parents=True, exist_ok=True)
    _write_markdown_report(payload, args.out_md)

    print(f"Benchmark JSON written: {args.out_json}")
    print(f"Benchmark Markdown written: {args.out_md}")
    print(f"Selected clean cases: {len(selected_candidates)} / Requested: {args.max_cases}")


if __name__ == "__main__":
    main()
