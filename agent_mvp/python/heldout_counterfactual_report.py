import argparse
from pathlib import Path
from typing import Any, Dict, List

import torch

from counterfactual_eval_world_model import (
    COUNTERFACTUAL_METRICS,
    _build_group_examples,
    _coverage_status,
    _group_counterfactual_candidates,
    _metric_pair_summary,
    _pairwise_counterfactual_records,
)
from data_io import write_json
from train_world_model import collect_prediction_records, compute_reward_norm_stats, load_split_records
from world_model import WorldModelNet


def _evaluate_split(
    model: WorldModelNet,
    records: List[Dict[str, Any]],
    batch_size: int,
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
    horizons: List[str],
    state_step: int,
    max_examples: int,
) -> Dict[str, Any]:
    if not records:
        empty_metrics = {key: _metric_pair_summary([], spec) for key, spec in COUNTERFACTUAL_METRICS.items()}
        return {
            "num_records": 0,
            "num_prediction_records": 0,
            "num_groups": 0,
            "num_pairs": 0,
            "metrics": empty_metrics,
            "focus_metrics": {
                "event_red_fire_count": empty_metrics.get("event_red_fire_count", {}),
                "termination_flag": empty_metrics.get("termination_flag", {}),
                "reward_red": empty_metrics.get("reward_red", {}),
            },
            "group_examples": [],
        }

    _, prediction_records = collect_prediction_records(
        model,
        records,
        batch_size,
        device,
        reward_norm_stats=reward_norm_stats,
    )
    groups = _group_counterfactual_candidates(prediction_records, state_step=state_step, horizons=horizons)

    pairwise_records: List[Dict[str, Any]] = []
    for group_records in groups.values():
        pairwise_records.extend(_pairwise_counterfactual_records(group_records))

    metrics = {
        metric_name: _metric_pair_summary(pairwise_records, metric_spec)
        for metric_name, metric_spec in COUNTERFACTUAL_METRICS.items()
    }

    return {
        "num_records": len(records),
        "num_prediction_records": len(prediction_records),
        "num_groups": len(groups),
        "num_pairs": len(pairwise_records),
        "metrics": metrics,
        "focus_metrics": {
            "event_red_fire_count": metrics.get("event_red_fire_count", {}),
            "termination_flag": metrics.get("termination_flag", {}),
            "reward_red": metrics.get("reward_red", {}),
        },
        "group_examples": _build_group_examples(groups, max_examples=max_examples),
    }


def _build_markdown(payload: Dict[str, Any]) -> str:
    heldout = payload["heldout_eval"]
    visible = payload["train_visible_eval"]
    lines: List[str] = []
    lines.append("# Held-out Counterfactual Validation Report")
    lines.append("")
    lines.append("## Verdict")
    lines.append(f"- valid: {payload['valid']}")
    lines.append(f"- coverage_insufficient: {payload['coverage_insufficient']}")
    lines.append(f"- min_groups: {payload['min_groups']}")
    lines.append(f"- min_pairs: {payload['min_pairs']}")
    lines.append(f"- heldout_groups: {heldout['num_groups']}")
    lines.append(f"- heldout_pairs: {heldout['num_pairs']}")
    lines.append("")
    lines.append("## Grouping")
    lines.append(f"- canonical_grouping_unified: {payload['canonical_grouping_unified']}")
    lines.append(f"- same_initial_signature: {payload['grouping_definition']['same_initial_key']}")
    lines.append(f"- group_key_fields: {payload['grouping_definition']['group_key_fields']}")
    lines.append("")
    lines.append("## Train-visible vs Held-out")
    lines.append(f"- train_visible_records: {visible['num_records']}")
    lines.append(f"- train_visible_groups: {visible['num_groups']}")
    lines.append(f"- train_visible_pairs: {visible['num_pairs']}")
    lines.append(f"- heldout_records: {heldout['num_records']}")
    lines.append(f"- heldout_groups: {heldout['num_groups']}")
    lines.append(f"- heldout_pairs: {heldout['num_pairs']}")
    lines.append("")
    lines.append("## Focus Metrics (Held-out)")
    for metric_name in ("event_red_fire_count", "termination_flag", "reward_red"):
        metric = heldout["focus_metrics"].get(metric_name, {})
        lines.append(
            f"- {metric_name}: delta_mae={metric.get('delta_mae')} | "
            f"signal_pairs={metric.get('num_signal_pairs')} | "
            f"sign_acc={metric.get('sign_accuracy_on_signal_pairs')}"
        )
    lines.append("")
    lines.append("## Evaluation Semantics")
    lines.append("- reward metrics: denormalized raw reward delta")
    lines.append("- termination metric: binary termination flag delta")
    lines.append("- termination reason: not part of counterfactual delta metric")
    lines.append("")
    if payload["coverage_insufficient"]:
        lines.append("## Conclusion")
        lines.append("- Held-out counterfactual result is INVALID for this run due to insufficient coverage.")
        lines.append("")
    else:
        lines.append("## Conclusion")
        lines.append("- Held-out counterfactual result is VALID for this run.")
        lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate held-out counterfactual validation report.")
    parser.add_argument("--pack-dir", type=Path, required=True, help="Directory containing processed/train_visible_cf.jsonl and processed/heldout_cf.jsonl")
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--out-json", type=Path, required=True)
    parser.add_argument("--out-md", type=Path, required=True)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument("--horizons", type=str, default="20,terminal")
    parser.add_argument("--state-step", type=int, default=0)
    parser.add_argument("--min-groups", type=int, default=5)
    parser.add_argument("--min-pairs", type=int, default=20)
    parser.add_argument("--max-examples", type=int, default=6)
    args = parser.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    horizons = [token.strip() for token in args.horizons.split(",") if token.strip()]

    train_visible_records = load_split_records(args.pack_dir, "train_visible_cf")
    heldout_records = load_split_records(args.pack_dir, "heldout_cf")
    reward_source = train_visible_records if train_visible_records else heldout_records
    reward_norm_stats = compute_reward_norm_stats(reward_source)

    model = WorldModelNet().to(device)
    model.load_state_dict(torch.load(args.model_path, map_location=device))

    visible_eval = _evaluate_split(
        model,
        train_visible_records,
        batch_size=args.batch_size,
        device=device,
        reward_norm_stats=reward_norm_stats,
        horizons=horizons,
        state_step=args.state_step,
        max_examples=args.max_examples,
    )
    heldout_eval = _evaluate_split(
        model,
        heldout_records,
        batch_size=args.batch_size,
        device=device,
        reward_norm_stats=reward_norm_stats,
        horizons=horizons,
        state_step=args.state_step,
        max_examples=args.max_examples,
    )

    payload: Dict[str, Any] = {
        "pack_dir": str(args.pack_dir),
        "model_path": str(args.model_path),
        "canonical_grouping_unified": True,
        "grouping_definition": {
            "same_initial_key": "canonical_state_signature(state_t)",
            "group_key_fields": ["state_signature", "horizon", "state_step"],
            "different_tactic_key": "tactic_combo_key",
        },
        "evaluation_semantics": {
            "reward_space": "denormalized_raw_reward",
            "termination": {
                "metric": "binary_termination_flag",
                "termination_reason": "not_used_for_delta_metrics",
            },
        },
        "train_visible_eval": visible_eval,
        "heldout_eval": heldout_eval,
    }
    payload.update(
        _coverage_status(
            num_groups=heldout_eval["num_groups"],
            num_pairs=heldout_eval["num_pairs"],
            min_groups=args.min_groups,
            min_pairs=args.min_pairs,
        )
    )

    write_json(args.out_json, payload)
    args.out_md.parent.mkdir(parents=True, exist_ok=True)
    args.out_md.write_text(_build_markdown(payload), encoding="utf-8")

    print(
        "Held-out report | "
        f"valid={payload['valid']} | "
        f"heldout_groups={heldout_eval['num_groups']} | "
        f"heldout_pairs={heldout_eval['num_pairs']}"
    )


if __name__ == "__main__":
    main()
