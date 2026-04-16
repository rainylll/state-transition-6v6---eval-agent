import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Dict, List, Tuple

import torch

from data_io import write_json
from train_world_model import collect_prediction_records, compute_reward_norm_stats, load_split_records
from world_model import WorldModelNet


def _sample_id(record: Dict[str, Any]) -> str:
    meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
    target_anchor = record.get("target_step", meta.get("target_step"))
    if target_anchor is None:
        target_anchor = record.get("target_index", meta.get("target_index", "unknown"))
    return "|".join(
        [
            str(record.get("episode_id", meta.get("episode_id", "unknown"))),
            str(record.get("horizon", "unknown")),
            str(record.get("t_index", meta.get("t_index", "unknown"))),
            str(target_anchor),
        ]
    )


def _build_model_records(
    model_path: Path,
    records: List[Dict[str, Any]],
    batch_size: int,
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
) -> Tuple[Dict[str, Any], Dict[str, Dict[str, Any]]]:
    model = WorldModelNet()
    model.load_state_dict(torch.load(model_path, map_location=device))
    model.to(device)
    summary, prediction_records = collect_prediction_records(
        model,
        records,
        batch_size=batch_size,
        device=device,
        reward_norm_stats=reward_norm_stats,
    )
    indexed = {_sample_id(item): item for item in prediction_records}
    return summary, indexed


def _to_bool(value: Any) -> bool:
    return bool(value) if value is not None else False


def _primary_bucket(sample: Dict[str, Any]) -> str:
    if (
        str(sample.get("horizon")) == "terminal"
        and _to_bool(sample.get("true_event_termination"))
        and _to_bool(sample.get("true_event_red_first_kill"))
        and _to_bool(sample.get("true_event_red_objective_complete"))
    ):
        return "terminal_red_first_kill_outcome_confusion"
    if (
        str(sample.get("horizon")) == "terminal"
        and _to_bool(sample.get("true_event_termination"))
        and _to_bool(sample.get("true_event_blue_objective_complete"))
        and not _to_bool(sample.get("true_event_red_first_kill"))
    ):
        return "terminal_blue_objective_without_first_kill"
    if not _to_bool(sample.get("future_any_first_kill_possible")):
        return "terminal_or_late_window_without_any_first_kill_remaining"
    if _to_bool(sample.get("prior_kill_seen")):
        return "post_first_kill_ineligible_window"
    if _to_bool(sample.get("future_red_first_kill_possible")) and not _to_bool(sample.get("future_blue_first_kill_possible")):
        return "red_first_kill_future_confusion"
    if _to_bool(sample.get("true_event_termination")) or _to_bool(sample.get("true_event_red_objective_complete")) or _to_bool(sample.get("true_event_blue_objective_complete")):
        return "termination_or_objective_overlap"
    if float(sample.get("true_event_red_kill_count", 0.0)) > 0.0 or float(sample.get("true_event_blue_kill_count", 0.0)) > 0.0:
        return "kill_delta_overlap"
    if _to_bool(sample.get("future_blue_first_kill_possible")) and not _to_bool(sample.get("true_event_blue_first_kill")):
        return "blue_first_kill_happens_later_outside_current_window"
    return "other_negative_window"


def _near_terminal_bucket(sample: Dict[str, Any]) -> str:
    if str(sample.get("horizon")) == "terminal":
        return "terminal_horizon"
    if _to_bool(sample.get("target_done")):
        return "target_done_before_terminal"
    remaining_views = sample.get("remaining_views")
    if remaining_views is None:
        return "unknown"
    remaining_views = int(remaining_views)
    if remaining_views <= 0:
        return "remaining_0"
    if remaining_views <= 20:
        return "remaining_le_20"
    if remaining_views <= 50:
        return "remaining_21_50"
    return "remaining_gt_50"


def _enrich_record(sample: Dict[str, Any]) -> Dict[str, Any]:
    episode_num_views = sample.get("episode_num_views")
    target_index = sample.get("target_index")
    remaining_views = None
    if episode_num_views is not None and target_index is not None:
        remaining_views = max(int(episode_num_views) - 1 - int(target_index), 0)

    out = dict(sample)
    out["future_any_first_kill_possible"] = _to_bool(sample.get("future_red_first_kill_possible")) or _to_bool(
        sample.get("future_blue_first_kill_possible")
    )
    out["remaining_views"] = remaining_views
    out["near_terminal_bucket"] = _near_terminal_bucket(out)
    out["primary_bucket"] = _primary_bucket(out)
    return out


def _base_case_from_record(record: Dict[str, Any], raw_record: Dict[str, Any]) -> Dict[str, Any]:
    raw_meta = raw_record.get("meta", {}) if isinstance(raw_record.get("meta", {}), dict) else {}
    return {
        "sample_id": _sample_id(record),
        "episode_id": record.get("episode_id"),
        "split_group_key": record.get("split_group_key"),
        "state_signature": record.get("state_signature"),
        "horizon": record.get("horizon"),
        "t_index": int(record.get("t_index") or 0),
        "target_index": int(raw_meta.get("target_index") or 0),
        "state_step": int(record.get("state_step") or 0),
        "target_step": int(record.get("target_step") or 0),
        "target_done": _to_bool(raw_meta.get("target_done")),
        "target_termination_reason": raw_meta.get("target_termination_reason"),
        "episode_num_views": int(raw_meta.get("episode_num_views") or 0),
        "episode_elapsed_steps": int(raw_meta.get("episode_elapsed_steps") or 0),
        "tactic_pair_key": record.get("tactic_pair_key"),
        "tactic_combo_key": record.get("tactic_combo_key"),
        "red_tactic_id": record.get("red_tactic_id"),
        "blue_tactic_id": record.get("blue_tactic_id"),
        "force_size_key": record.get("force_size_key"),
        "red_attr_bucket": record.get("red_attr_bucket"),
        "blue_attr_bucket": record.get("blue_attr_bucket"),
        "pred_event_blue_first_kill_prob": float(record.get("pred_event_blue_first_kill_prob", 0.0)),
        "true_event_blue_first_kill": float(record.get("true_event_blue_first_kill", 0.0)),
        "pred_event_red_first_kill_prob": float(record.get("pred_event_red_first_kill_prob", 0.0)),
        "true_event_red_first_kill": float(record.get("true_event_red_first_kill", 0.0)),
        "pred_event_blue_objective_complete_prob": float(record.get("pred_event_blue_objective_complete_prob", 0.0)),
        "true_event_blue_objective_complete": float(record.get("true_event_blue_objective_complete", 0.0)),
        "pred_event_red_objective_complete_prob": float(record.get("pred_event_red_objective_complete_prob", 0.0)),
        "true_event_red_objective_complete": float(record.get("true_event_red_objective_complete", 0.0)),
        "pred_event_termination_prob": float(record.get("pred_event_termination_prob", 0.0)),
        "true_event_termination": float(record.get("true_event_termination", 0.0)),
        "true_event_red_kill_count": float(record.get("true_event_red_kill_count", 0.0)),
        "true_event_blue_kill_count": float(record.get("true_event_blue_kill_count", 0.0)),
        "blue_first_kill_pred_positive_rate": float(record.get("blue_first_kill_pred_positive_rate", 0.0)),
        "blue_first_kill_false_positive_rate": record.get("blue_first_kill_false_positive_rate"),
        "prior_kill_seen": _to_bool(record.get("prior_kill_seen", raw_meta.get("prior_kill_seen"))),
        "red_first_kill_eligible": _to_bool(record.get("red_first_kill_eligible", raw_meta.get("red_first_kill_eligible"))),
        "blue_first_kill_eligible": _to_bool(record.get("blue_first_kill_eligible", raw_meta.get("blue_first_kill_eligible"))),
        "future_red_first_kill_possible": _to_bool(record.get("future_red_first_kill_possible", raw_meta.get("future_red_first_kill_possible"))),
        "future_blue_first_kill_possible": _to_bool(record.get("future_blue_first_kill_possible", raw_meta.get("future_blue_first_kill_possible"))),
    }


def _bucket_summary(records: List[Dict[str, Any]], total: int) -> Dict[str, Any]:
    counter = Counter(item["primary_bucket"] for item in records)
    output: Dict[str, Any] = {}
    for key, count in counter.most_common():
        output[key] = {
            "count": int(count),
            "share_of_blue_false_positives": float(count / total) if total else 0.0,
        }
    return output


def _dimension_counter(records: List[Dict[str, Any]], field: str) -> Dict[str, Any]:
    counter = Counter(str(item.get(field, "unknown")) for item in records)
    total = len(records)
    return {
        key: {
            "count": int(value),
            "share": float(value / total) if total else 0.0,
        }
        for key, value in counter.most_common()
    }


def _overlap_counter(records: List[Dict[str, Any]]) -> Dict[str, Any]:
    checks = {
        "prior_kill_seen": lambda x: _to_bool(x.get("prior_kill_seen")),
        "future_red_first_kill_possible": lambda x: _to_bool(x.get("future_red_first_kill_possible")),
        "future_blue_first_kill_possible": lambda x: _to_bool(x.get("future_blue_first_kill_possible")),
        "termination_overlap": lambda x: _to_bool(x.get("true_event_termination")),
        "objective_overlap": lambda x: _to_bool(x.get("true_event_red_objective_complete")) or _to_bool(x.get("true_event_blue_objective_complete")),
        "kill_delta_overlap": lambda x: float(x.get("true_event_red_kill_count", 0.0)) > 0.0 or float(x.get("true_event_blue_kill_count", 0.0)) > 0.0,
    }
    total = len(records)
    output: Dict[str, Any] = {}
    for key, fn in checks.items():
        count = sum(1 for item in records if fn(item))
        output[key] = {
            "count": int(count),
            "share": float(count / total) if total else 0.0,
        }
    return output


def _compare_models_by_bucket(
    bucket_records: List[Dict[str, Any]],
    model_predictions: Dict[str, Dict[str, Dict[str, Any]]],
) -> Dict[str, Any]:
    sample_ids = [item["sample_id"] for item in bucket_records]
    output: Dict[str, Any] = {}
    for model_name, mapping in model_predictions.items():
        selected = [mapping[sid] for sid in sample_ids if sid in mapping]
        if not selected:
            continue
        pred_positive_count = sum(1 for item in selected if float(item.get("pred_event_blue_first_kill_prob", 0.0)) >= 0.5)
        false_positive_count = sum(
            1
            for item in selected
            if float(item.get("pred_event_blue_first_kill_prob", 0.0)) >= 0.5 and float(item.get("true_event_blue_first_kill", 0.0)) < 0.5
        )
        output[model_name] = {
            "num_samples": len(selected),
            "mean_pred_blue_first_kill_prob": float(
                sum(float(item.get("pred_event_blue_first_kill_prob", 0.0)) for item in selected) / len(selected)
            ),
            "pred_positive_rate": float(pred_positive_count / len(selected)),
            "false_positive_rate": float(false_positive_count / len(selected)),
        }
    return output


def _select_examples(records: List[Dict[str, Any]], limit: int = 5) -> List[Dict[str, Any]]:
    ordered = sorted(records, key=lambda item: float(item.get("pred_event_blue_first_kill_prob", 0.0)), reverse=True)
    return ordered[:limit]


def _build_markdown(report: Dict[str, Any]) -> str:
    lines: List[str] = []
    lines.append("# Phase A Blue False-Positive Diagnosis")
    lines.append("")
    lines.append("## Scope")
    lines.append(f"- Primary model: `{report['primary_model']['name']}`")
    lines.append(f"- Split: `{report['dataset']['split']}`")
    lines.append(f"- Records analyzed: `{report['dataset']['num_records']}`")
    lines.append(f"- Blue false positives: `{report['blue_false_positive_summary']['count']}`")
    lines.append("")
    lines.append("## Key Finding")
    lines.append(report["summary"]["topline"])
    lines.append("")
    lines.append("## Primary Buckets")
    for bucket_name, stats in report["primary_bucket_distribution"].items():
        lines.append(
            f"- `{bucket_name}`: {stats['count']} cases ({stats['share_of_blue_false_positives']:.1%})"
        )
    lines.append("")
    lines.append("## Overlap Signals")
    for key, stats in report["overlap_signals"].items():
        lines.append(f"- `{key}`: {stats['count']} cases ({stats['share']:.1%})")
    lines.append("")
    lines.append("## Horizon / Near-Terminal")
    for key, stats in report["dimension_distributions"]["near_terminal_bucket"].items():
        lines.append(f"- `{key}`: {stats['count']} cases ({stats['share']:.1%})")
    lines.append("")
    lines.append("## Why 10.9 / 10.10 Can Improve Sign But Hurt Strict")
    for item in report["summary"]["sign_vs_strict_explanation"]:
        lines.append(f"- {item}")
    lines.append("")
    lines.append("## Bucket Diagnostics")
    for bucket_name, bucket in report["bucket_details"].items():
        lines.append(f"### {bucket_name}")
        lines.append(
            f"- Count: {bucket['count']} ({bucket['share_of_blue_false_positives']:.1%} of blue false positives)"
        )
        lines.append(f"- Interpretation: {bucket['interpretation']}")
        lines.append("- Model comparison:")
        for model_name, stats in bucket["model_comparison"].items():
            lines.append(
                f"  - `{model_name}`: mean_prob={stats['mean_pred_blue_first_kill_prob']:.4f}, "
                f"pred_positive_rate={stats['pred_positive_rate']:.3f}, false_positive_rate={stats['false_positive_rate']:.3f}"
            )
        if bucket["examples"]:
            lines.append("- Example cases:")
            for example in bucket["examples"]:
                lines.append(
                    f"  - `{example['sample_id']}` | horizon={example['horizon']} | "
                    f"prob={example['pred_event_blue_first_kill_prob']:.4f} | "
                    f"future_red={example['future_red_first_kill_possible']} | "
                    f"future_blue={example['future_blue_first_kill_possible']} | "
                    f"prior_kill_seen={example['prior_kill_seen']} | "
                    f"termination={example['true_event_termination']} | "
                    f"objective_red={example['true_event_red_objective_complete']} | "
                    f"objective_blue={example['true_event_blue_objective_complete']}"
                )
        lines.append("")
    lines.append("## Reviewer Recommendation")
    lines.append(f"- {report['summary']['next_cut']}")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Diagnose blue first-kill false positives on held-out strict eval.")
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--split", type=str, default="heldout_cf")
    parser.add_argument("--primary-model-name", type=str, default="phase10_8")
    parser.add_argument("--primary-model-path", type=Path, required=True)
    parser.add_argument("--compare-model", action="append", default=[], help="Format name=path")
    parser.add_argument("--device", type=str, default="cuda")
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--out-json", type=Path, required=True)
    parser.add_argument("--out-md", type=Path, required=True)
    parser.add_argument("--out-cases-json", type=Path, required=True)
    args = parser.parse_args()

    device = torch.device(args.device)
    records = load_split_records(args.data_dir, args.split)
    reward_norm_stats = compute_reward_norm_stats(records)
    raw_record_map = {_sample_id(record): record for record in records}

    model_specs = [(args.primary_model_name, args.primary_model_path)]
    for item in args.compare_model:
        if "=" not in item:
            raise ValueError(f"Invalid --compare-model value: {item}")
        name, path = item.split("=", 1)
        model_specs.append((name.strip(), Path(path.strip())))

    model_summaries: Dict[str, Dict[str, Any]] = {}
    model_predictions: Dict[str, Dict[str, Dict[str, Any]]] = {}
    for name, model_path in model_specs:
        summary, mapping = _build_model_records(
            model_path=model_path,
            records=records,
            batch_size=args.batch_size,
            device=device,
            reward_norm_stats=reward_norm_stats,
        )
        model_summaries[name] = summary
        model_predictions[name] = mapping

    primary_predictions = model_predictions[args.primary_model_name]
    enriched_cases: List[Dict[str, Any]] = []
    for sample_id, record in primary_predictions.items():
        if float(record.get("pred_event_blue_first_kill_prob", 0.0)) < 0.5:
            continue
        if float(record.get("true_event_blue_first_kill", 0.0)) >= 0.5:
            continue
        raw_record = raw_record_map.get(sample_id, {})
        enriched = _enrich_record(_base_case_from_record(record, raw_record))
        enriched_cases.append(enriched)

    total_fp = len(enriched_cases)
    primary_bucket_distribution = _bucket_summary(enriched_cases, total_fp)
    dimension_distributions = {
        "horizon": _dimension_counter(enriched_cases, "horizon"),
        "t_index": _dimension_counter(enriched_cases, "t_index"),
        "near_terminal_bucket": _dimension_counter(enriched_cases, "near_terminal_bucket"),
    }
    overlap_signals = _overlap_counter(enriched_cases)

    bucket_order = [key for key in primary_bucket_distribution.keys()]
    bucket_details: Dict[str, Any] = {}
    cases_export: Dict[str, Any] = {}
    for bucket_name in bucket_order:
        bucket_records = [item for item in enriched_cases if item["primary_bucket"] == bucket_name]
        model_comparison = _compare_models_by_bucket(bucket_records, model_predictions)
        examples = _select_examples(bucket_records)
        if bucket_name == "terminal_red_first_kill_outcome_confusion":
            interpretation = "Terminal windows where red owns the decisive kill/objective chain, but blue first-kill is still predicted."
        elif bucket_name == "terminal_blue_objective_without_first_kill":
            interpretation = "Terminal windows where blue objective completes, but there is still no blue first-kill; the model appears to conflate blue success with blue first-kill."
        elif bucket_name == "terminal_or_late_window_without_any_first_kill_remaining":
            interpretation = "No side can still produce a first-kill in the future of this window, so a blue first-kill prediction is a pure late-window eligibility error."
        elif bucket_name == "post_first_kill_ineligible_window":
            interpretation = "The episode is already past the first-kill point, but blue first-kill is still being predicted."
        elif bucket_name == "red_first_kill_future_confusion":
            interpretation = "Blue prediction is firing in windows where red is the side that still owns the future first-kill path."
        elif bucket_name == "termination_or_objective_overlap":
            interpretation = "Blue first-kill is being confused with decisive terminal/objective transition windows."
        elif bucket_name == "kill_delta_overlap":
            interpretation = "Blue first-kill is being confused with generic kill activity rather than the specific first-kill qualification."
        elif bucket_name == "blue_first_kill_happens_later_outside_current_window":
            interpretation = "The model may be reacting to a real future blue first-kill, but it is firing too early for the current horizon."
        else:
            interpretation = "Residual bucket without a stronger structural explanation."
        bucket_details[bucket_name] = {
            "count": len(bucket_records),
            "share_of_blue_false_positives": float(len(bucket_records) / total_fp) if total_fp else 0.0,
            "interpretation": interpretation,
            "model_comparison": model_comparison,
            "examples": examples,
        }
        cases_export[bucket_name] = examples

    top_buckets = bucket_order[:3]
    sign_vs_strict_explanation = [
        "The dominant blue false-positive buckets are windows where blue first-kill should not be active yet or is no longer eligible, so pushing blue logits upward can improve pairwise direction on some counterfactual comparisons while still crossing the 0.5 strict threshold too often.",
        "If 10.9/10.10 raise blue first-kill probability in red-future or lost-eligibility windows, counterfactual sign can improve without improving strict precision.",
        "Because held-out strict uses only state_step=0 groups, t_index is degenerate here; the real failure mode is not temporal spread inside the episode but misclassification of horizon/eligibility semantics from the same initial state.",
    ]

    report = {
        "phase": "A",
        "title": "Blue false-positive attribution diagnosis",
        "dataset": {
            "data_dir": str(args.data_dir),
            "split": args.split,
            "num_records": len(records),
            "grouping_note": "heldout_cf is built from same-initial groups at state_step=0 with horizons 20 and terminal",
        },
        "primary_model": {
            "name": args.primary_model_name,
            "path": str(args.primary_model_path),
            "strict_summary": model_summaries[args.primary_model_name],
        },
        "compare_models": {
            name: {
                "path": str(path),
                "strict_summary": model_summaries[name],
            }
            for name, path in model_specs
            if name != args.primary_model_name
        },
        "blue_false_positive_summary": {
            "count": total_fp,
            "share_within_all_samples": float(total_fp / len(records)) if records else 0.0,
        },
        "primary_bucket_distribution": primary_bucket_distribution,
        "dimension_distributions": dimension_distributions,
        "overlap_signals": overlap_signals,
        "bucket_details": bucket_details,
        "summary": {
            "topline": (
                f"10.8 blue false positives are dominated by {', '.join(top_buckets)}"
                if top_buckets
                else "No blue false positives found."
            ),
            "top_buckets": top_buckets,
            "sign_vs_strict_explanation": sign_vs_strict_explanation,
            "next_cut": (
                "Next cut should target blue false-positive buckets with side-aware eligibility or false-positive filtering, not larger repeat or broader critical-group reweighting."
            ),
        },
        "notes": {
            "t_index_limitation": "All heldout_cf records use state_step=0, so t_index is effectively degenerate for this pack.",
            "eligibility_caveat": "Current processed meta can support coarse prior_kill_seen / future side possible analysis, but this does not yet prove a fully side-aware eligibility design.",
        },
    }

    args.out_json.parent.mkdir(parents=True, exist_ok=True)
    args.out_md.parent.mkdir(parents=True, exist_ok=True)
    args.out_cases_json.parent.mkdir(parents=True, exist_ok=True)
    write_json(args.out_json, report)
    write_json(args.out_cases_json, cases_export)
    args.out_md.write_text(_build_markdown(report), encoding="utf-8")


if __name__ == "__main__":
    main()
