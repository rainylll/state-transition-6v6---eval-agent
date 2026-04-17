import argparse
from collections import defaultdict
from itertools import combinations
from pathlib import Path
from typing import Any, Dict, List, Sequence, Tuple

import torch

from data_io import write_json
from train_world_model import collect_prediction_records, compute_reward_norm_stats, load_split_records
from world_model import WorldModelNet

COUNTERFACTUAL_METRICS = {
    "event_red_first_contact_prob": {
        "pred_key": "pred_event_red_first_contact_prob",
        "true_key": "true_event_red_first_contact",
        "signal_threshold": 0.5,
    },
    "event_blue_first_contact_prob": {
        "pred_key": "pred_event_blue_first_contact_prob",
        "true_key": "true_event_blue_first_contact",
        "signal_threshold": 0.5,
    },
    "event_red_first_fire_prob": {
        "pred_key": "pred_event_red_first_fire_prob",
        "true_key": "true_event_red_first_fire",
        "signal_threshold": 0.5,
    },
    "event_blue_first_fire_prob": {
        "pred_key": "pred_event_blue_first_fire_prob",
        "true_key": "true_event_blue_first_fire",
        "signal_threshold": 0.5,
    },
    "event_red_fire_count": {
        "pred_key": "pred_event_red_fire_count",
        "true_key": "true_event_red_fire_count",
        "signal_threshold": 0.5,
    },
    "event_blue_fire_count": {
        "pred_key": "pred_event_blue_fire_count",
        "true_key": "true_event_blue_fire_count",
        "signal_threshold": 0.5,
    },
    "event_red_kill_count": {
        "pred_key": "pred_event_red_kill_count",
        "true_key": "true_event_red_kill_count",
        "signal_threshold": 0.5,
    },
    "event_blue_kill_count": {
        "pred_key": "pred_event_blue_kill_count",
        "true_key": "true_event_blue_kill_count",
        "signal_threshold": 0.5,
    },
    "event_red_first_kill_prob": {
        "pred_key": "pred_event_red_first_kill_prob",
        "true_key": "true_event_red_first_kill",
        "signal_threshold": 0.5,
    },
    "event_blue_first_kill_prob": {
        "pred_key": "pred_event_blue_first_kill_prob",
        "true_key": "true_event_blue_first_kill",
        "signal_threshold": 0.5,
    },
    "termination_flag": {
        "pred_key": "pred_event_termination_prob",
        "true_key": "true_event_termination",
        "signal_threshold": 0.5,
    },
    "reward_red": {
        "pred_key": "pred_reward_red",
        "true_key": "true_reward_red",
        "signal_threshold": 0.25,
    },
    "reward_blue": {
        "pred_key": "pred_reward_blue",
        "true_key": "true_reward_blue",
        "signal_threshold": 0.25,
    },
    "trajectory_red_alive_ratio": {
        "pred_key": "pred_target_red_alive_ratio",
        "true_key": "true_target_red_alive_ratio",
        "signal_threshold": 0.05,
    },
    "trajectory_blue_alive_ratio": {
        "pred_key": "pred_target_blue_alive_ratio",
        "true_key": "true_target_blue_alive_ratio",
        "signal_threshold": 0.05,
    },
    "trajectory_red_mean_missile": {
        "pred_key": "pred_target_red_mean_missile",
        "true_key": "true_target_red_mean_missile",
        "signal_threshold": 0.25,
    },
    "trajectory_blue_mean_missile": {
        "pred_key": "pred_target_blue_mean_missile",
        "true_key": "true_target_blue_mean_missile",
        "signal_threshold": 0.25,
    },
    "terminal_red_win_prob": {
        "pred_key": "pred_red_win_prob",
        "true_key": "true_red_win",
        "signal_threshold": 0.5,
    },
    "terminal_red_alive_ratio": {
        "pred_key": "pred_terminal_red_alive_ratio",
        "true_key": "true_terminal_red_alive_ratio",
        "signal_threshold": 0.05,
    },
    "terminal_blue_alive_ratio": {
        "pred_key": "pred_terminal_blue_alive_ratio",
        "true_key": "true_terminal_blue_alive_ratio",
        "signal_threshold": 0.05,
    },
    "terminal_red_mean_missile": {
        "pred_key": "pred_terminal_red_mean_missile",
        "true_key": "true_terminal_red_mean_missile",
        "signal_threshold": 0.25,
    },
    "terminal_blue_mean_missile": {
        "pred_key": "pred_terminal_blue_mean_missile",
        "true_key": "true_terminal_blue_mean_missile",
        "signal_threshold": 0.25,
    },
}


def _mean(values: Sequence[float]) -> float:
    if not values:
        return 0.0
    return float(sum(values) / len(values))


def _metric_pair_summary(
    pair_records: Sequence[Dict[str, Any]],
    metric_spec: Dict[str, Any],
) -> Dict[str, Any]:
    pred_key = metric_spec["pred_key"]
    true_key = metric_spec["true_key"]
    threshold = float(metric_spec["signal_threshold"])

    abs_delta_errors: List[float] = []
    sign_correct = 0
    signal_pairs = 0
    real_abs_deltas: List[float] = []
    pred_abs_deltas: List[float] = []

    for pair_record in pair_records:
        pred_delta = float(pair_record[pred_key])
        true_delta = float(pair_record[true_key])
        abs_delta_errors.append(abs(pred_delta - true_delta))
        real_abs_deltas.append(abs(true_delta))
        pred_abs_deltas.append(abs(pred_delta))
        if abs(true_delta) >= threshold:
            signal_pairs += 1
            if pred_delta == 0.0:
                if true_delta == 0.0:
                    sign_correct += 1
            elif (pred_delta > 0.0) == (true_delta > 0.0):
                sign_correct += 1

    return {
        "num_pairs": len(pair_records),
        "num_signal_pairs": signal_pairs,
        "delta_mae": _mean(abs_delta_errors),
        "mean_true_abs_delta": _mean(real_abs_deltas),
        "mean_pred_abs_delta": _mean(pred_abs_deltas),
        "sign_accuracy_on_signal_pairs": (sign_correct / signal_pairs) if signal_pairs > 0 else None,
    }


def _pairwise_counterfactual_records(group_records: Sequence[Dict[str, Any]]) -> List[Dict[str, Any]]:
    pairwise_records: List[Dict[str, Any]] = []
    for left, right in combinations(group_records, 2):
        if left.get("tactic_combo_key") == right.get("tactic_combo_key"):
            continue
        pair_record: Dict[str, Any] = {
            "left_episode_id": left.get("episode_id"),
            "right_episode_id": right.get("episode_id"),
            "left_tactic_combo_key": left.get("tactic_combo_key"),
            "right_tactic_combo_key": right.get("tactic_combo_key"),
            "horizon": left.get("horizon"),
            "state_step": left.get("state_step"),
        }
        for metric_name, metric_spec in COUNTERFACTUAL_METRICS.items():
            pred_key = metric_spec["pred_key"]
            true_key = metric_spec["true_key"]
            pair_record[pred_key] = float(right[pred_key]) - float(left[pred_key])
            pair_record[true_key] = float(right[true_key]) - float(left[true_key])
        pairwise_records.append(pair_record)
    return pairwise_records


def _group_counterfactual_candidates(
    prediction_records: Sequence[Dict[str, Any]],
    state_step: int,
    horizons: Sequence[str],
) -> Dict[Tuple[str, str, int], List[Dict[str, Any]]]:
    groups: Dict[Tuple[str, str, int], List[Dict[str, Any]]] = defaultdict(list)
    requested_horizons = {str(horizon) for horizon in horizons}
    for record in prediction_records:
        record_state_step = record.get("state_step")
        if record_state_step is None:
            continue
        if int(record_state_step) != state_step:
            continue
        if str(record.get("horizon")) not in requested_horizons:
            continue
        group_key = (
            str(record.get("state_signature")),
            str(record.get("horizon")),
            int(record_state_step),
        )
        groups[group_key].append(record)
    return {
        group_key: records
        for group_key, records in groups.items()
        if len(records) >= 2 and len({record.get("tactic_combo_key") for record in records}) >= 2
    }


def _coverage_status(num_groups: int, num_pairs: int, min_groups: int, min_pairs: int) -> Dict[str, Any]:
    groups_ok = int(num_groups) >= int(min_groups)
    pairs_ok = int(num_pairs) >= int(min_pairs)
    valid = bool(groups_ok and pairs_ok)
    return {
        "valid": valid,
        "coverage_insufficient": not valid,
        "groups_ok": groups_ok,
        "pairs_ok": pairs_ok,
        "min_groups": int(min_groups),
        "min_pairs": int(min_pairs),
    }


def _build_group_examples(groups: Dict[Tuple[str, str, int], List[Dict[str, Any]]], max_examples: int) -> List[Dict[str, Any]]:
    examples: List[Dict[str, Any]] = []
    ordered_groups = sorted(groups.items(), key=lambda item: (-len(item[1]), item[0][1], item[0][2]))
    for (state_signature, horizon, state_step), records in ordered_groups[:max_examples]:
        examples.append(
            {
                "state_signature": state_signature,
                "horizon": horizon,
                "state_step": state_step,
                "num_variants": len(records),
                "variants": [
                    {
                        "episode_id": record.get("episode_id"),
                        "split_name": record.get("split_name"),
                        "tactic_combo_key": record.get("tactic_combo_key"),
                        "pred_target_red_alive_ratio": record.get("pred_target_red_alive_ratio"),
                        "true_target_red_alive_ratio": record.get("true_target_red_alive_ratio"),
                        "pred_target_blue_alive_ratio": record.get("pred_target_blue_alive_ratio"),
                        "true_target_blue_alive_ratio": record.get("true_target_blue_alive_ratio"),
                        "pred_red_win_prob": record.get("pred_red_win_prob"),
                        "true_red_win": record.get("true_red_win"),
                    }
                    for record in sorted(records, key=lambda item: str(item.get("tactic_combo_key")))
                ],
            }
        )
    return examples


def main() -> None:
    parser = argparse.ArgumentParser(description="Counterfactual evaluation for same-initial / different-tactic samples.")
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--out-path", type=Path, required=True)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument(
        "--splits",
        type=str,
        default="train,val,test,ood",
        help="Comma-separated processed splits to search for same-initial counterfactual groups.",
    )
    parser.add_argument(
        "--horizons",
        type=str,
        default="20,terminal",
        help="Comma-separated horizons to compare for counterfactual groups.",
    )
    parser.add_argument("--state-step", type=int, default=0, help="Only compare records from this initial state_step.")
    parser.add_argument("--max-examples", type=int, default=6)
    parser.add_argument("--min-groups", type=int, default=5)
    parser.add_argument("--min-pairs", type=int, default=20)
    parser.add_argument(
        "--allow-insufficient-coverage",
        action="store_true",
        help="If set, do not fail process exit code when coverage is below min-groups/min-pairs.",
    )
    args = parser.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    split_names = [token.strip() for token in args.splits.split(",") if token.strip()]
    horizons = [token.strip() for token in args.horizons.split(",") if token.strip()]
    records: List[Dict[str, Any]] = []
    loaded_splits: List[str] = []
    for split_name in split_names:
        split_records = load_split_records(args.data_dir, split_name)
        if split_records:
            records.extend(split_records)
            loaded_splits.append(split_name)

    if not records:
        raise RuntimeError(f"No processed records found in {args.data_dir} for splits: {split_names}")

    model = WorldModelNet().to(device)
    model.load_state_dict(torch.load(args.model_path, map_location=device), strict=False)

    reward_source_records = load_split_records(args.data_dir, "train")
    if not reward_source_records:
        reward_source_records = records
    reward_norm_stats = compute_reward_norm_stats(reward_source_records)

    _, prediction_records = collect_prediction_records(
        model,
        records,
        args.batch_size,
        device,
        reward_norm_stats=reward_norm_stats,
    )
    groups = _group_counterfactual_candidates(prediction_records, args.state_step, horizons)

    pairwise_records: List[Dict[str, Any]] = []
    for group_records in groups.values():
        pairwise_records.extend(_pairwise_counterfactual_records(group_records))

    metrics_summary = {
        metric_name: _metric_pair_summary(pairwise_records, metric_spec)
        for metric_name, metric_spec in COUNTERFACTUAL_METRICS.items()
    }

    payload = {
        "data_dir": str(args.data_dir),
        "loaded_splits": loaded_splits,
        "horizons": horizons,
        "state_step": args.state_step,
        "grouping_definition": {
            "same_initial_key": "canonical_state_signature(state_t)",
            "group_key_fields": ["state_signature", "horizon", "state_step"],
            "different_tactic_key": "tactic_combo_key",
        },
        "evaluation_semantics": {
            "reward_space": {
                "pred_reward_red": "denormalized_raw_reward",
                "true_reward_red": "denormalized_raw_reward",
                "pred_reward_blue": "denormalized_raw_reward",
                "true_reward_blue": "denormalized_raw_reward",
            },
            "termination": {
                "termination_flag": "effective_combat_termination_flag",
                "effective_definition": "terminal horizon with non-decisive reasons (safety_limit/none/timeout/time_limit) mapped to 0",
            },
        },
        "num_records": len(records),
        "num_prediction_records": len(prediction_records),
        "num_counterfactual_groups": len(groups),
        "num_counterfactual_pairs": len(pairwise_records),
        "metrics": metrics_summary,
        "focus_metrics": {
            "event_red_fire_count": metrics_summary.get("event_red_fire_count", {}),
            "event_red_first_kill_prob": metrics_summary.get("event_red_first_kill_prob", {}),
            "event_blue_first_kill_prob": metrics_summary.get("event_blue_first_kill_prob", {}),
            "termination_flag": metrics_summary.get("termination_flag", {}),
            "reward_red": metrics_summary.get("reward_red", {}),
            "terminal_red_win_prob": metrics_summary.get("terminal_red_win_prob", {}),
        },
        "group_examples": _build_group_examples(groups, args.max_examples),
    }
    payload.update(
        _coverage_status(
            num_groups=payload["num_counterfactual_groups"],
            num_pairs=payload["num_counterfactual_pairs"],
            min_groups=args.min_groups,
            min_pairs=args.min_pairs,
        )
    )
    write_json(args.out_path, payload)

    print(
        "Counterfactual evaluation | "
        f"groups={payload['num_counterfactual_groups']} | "
        f"pairs={payload['num_counterfactual_pairs']} | "
        f"valid={payload['valid']}"
    )
    for metric_name in (
        "event_red_fire_count",
        "event_red_first_kill_prob",
        "event_blue_first_kill_prob",
        "termination_flag",
        "reward_red",
        "terminal_red_win_prob",
    ):
        metric = metrics_summary[metric_name]
        print(
            f"{metric_name}: "
            f"delta_mae={metric['delta_mae']:.4f} | "
            f"signal_pairs={metric['num_signal_pairs']} | "
            f"sign_acc={metric['sign_accuracy_on_signal_pairs']}"
        )

    if payload["coverage_insufficient"]:
        print(
            "Coverage insufficient for held-out counterfactual validity | "
            f"min_groups={payload['min_groups']} min_pairs={payload['min_pairs']}"
        )
        if not args.allow_insufficient_coverage:
            raise SystemExit(2)


if __name__ == "__main__":
    main()
