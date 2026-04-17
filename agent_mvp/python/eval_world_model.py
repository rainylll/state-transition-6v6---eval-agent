import argparse
from pathlib import Path
from typing import Any, Dict, List

import torch

from data_io import write_json
from train_world_model import compute_reward_norm_stats, load_split_records, run_strict_split_evaluation
from world_model import WorldModelNet


def _available_splits(data_dir: Path, requested_splits: List[str]) -> List[str]:
    available: List[str] = []
    for split_name in requested_splits:
        if load_split_records(data_dir, split_name):
            available.append(split_name)
    return available


def _print_split_summary(name: str, metrics: Dict[str, Any]) -> None:
    aggregate = metrics.get("aggregate", {})
    if not aggregate:
        return

    def _v(key: str) -> float:
        value = aggregate.get(key, 0.0)
        if value is None:
            return 0.0
        return float(value)

    print(
        f"{name:>9} | "
        f"loss={_v('loss_total'):.4f} | "
        f"traj_alive_acc={_v('trajectory_alive_accuracy'):.4f} | "
        f"traj_pos_err_m={_v('trajectory_position_error_m'):.1f} | "
        f"traj_heading_mae_deg={_v('trajectory_heading_mae_deg'):.1f} | "
        f"event_flag_acc={_v('event_flag_accuracy'):.4f} | "
        f"critical_event_acc={_v('critical_event_accuracy'):.4f} | "
        f"termination_acc={_v('termination_flag_accuracy'):.4f} | "
        f"termination_hi_conf_hit={_v('termination_flag_high_conf_hit_rate'):.4f} | "
        f"red_fk_acc={_v('red_first_kill_accuracy'):.4f} | "
        f"blue_fk_acc={_v('blue_first_kill_accuracy'):.4f} | "
        f"first_kill_acc={_v('first_kill_accuracy'):.4f} | "
        f"red_fk_recall={_v('red_first_kill_positive_recall'):.4f} | "
        f"blue_fk_recall={_v('blue_first_kill_positive_recall'):.4f} | "
        f"red_fk_prec={_v('red_first_kill_precision'):.4f} | "
        f"blue_fk_prec={_v('blue_first_kill_precision'):.4f} | "
        f"red_fk_hi_conf={_v('red_first_kill_high_conf_hit_rate'):.4f} | "
        f"blue_fk_hi_conf={_v('blue_first_kill_high_conf_hit_rate'):.4f} | "
        f"blue_fk_pred_pos={_v('blue_first_kill_pred_positive_rate'):.4f} | "
        f"blue_fk_fp={_v('blue_first_kill_false_positive_rate'):.4f} | "
        f"blue_fk_term_fp={_v('blue_first_kill_terminal_false_positive_rate'):.4f} | "
        f"term_red_conf={_v('terminal_red_outcome_confusion_rate'):.4f} | "
        f"term_blue_obj_conf={_v('terminal_blue_objective_confusion_rate'):.4f} | "
        f"blue_fk_elig_acc={_v('blue_first_kill_eligible_subset_accuracy'):.4f} | "
        f"blue_fk_elig_prec={_v('blue_first_kill_eligible_subset_precision'):.4f} | "
        f"objective_complete_acc={_v('objective_complete_accuracy'):.4f} | "
        f"reward_mae={_v('reward_total_mae'):.4f} | "
        f"reward_red_delta_scale={_v('reward_red_delta_scale'):.4f} | "
        f"reward_blue_delta_scale={_v('reward_blue_delta_scale'):.4f} | "
        f"win_acc={_v('terminal_win_accuracy'):.4f} | "
        f"consistency_mae={_v('consistency_total_mae'):.4f}"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Run stricter world-model evaluation on seen and OOD splits.")
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--out-path", type=Path, required=True)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument(
        "--splits",
        type=str,
        default="val,test,ood",
        help="Comma-separated processed splits to evaluate. seen_eval is built automatically from val+test if available.",
    )
    args = parser.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    requested_splits = [token.strip() for token in args.splits.split(",") if token.strip()]
    split_names = _available_splits(args.data_dir, requested_splits)

    reward_source_records = load_split_records(args.data_dir, "train")
    if not reward_source_records:
        for split_name in split_names:
            reward_source_records.extend(load_split_records(args.data_dir, split_name))
    reward_norm_stats = compute_reward_norm_stats(reward_source_records)

    model = WorldModelNet().to(device)
    model.load_state_dict(torch.load(args.model_path, map_location=device), strict=False)

    metrics = run_strict_split_evaluation(
        model,
        args.data_dir,
        args.batch_size,
        device,
        reward_norm_stats=reward_norm_stats,
        split_names=split_names,
    )
    metrics["_evaluation_semantics"] = {
        "reward_space": {
            "reward_red_mae": "denormalized_raw_reward",
            "reward_blue_mae": "denormalized_raw_reward",
            "reward_total_mae": "denormalized_raw_reward",
            "reward_red_delta_scale": "denormalized_raw_reward",
            "reward_blue_delta_scale": "denormalized_raw_reward",
        },
        "termination": {
            "termination_flag_accuracy": "effective_combat_termination_flag",
            "termination_flag_high_conf_hit_rate": "effective_combat_termination_flag_with_prob>=0.7",
            "effective_definition": "terminal horizon with non-decisive reasons (safety_limit/none/timeout/time_limit) mapped to 0",
        },
    }
    write_json(args.out_path, metrics)

    print("World-model strict evaluation:")
    for split_name in ("val", "test", "seen_eval", "ood"):
        if split_name in metrics:
            _print_split_summary(split_name, metrics[split_name])


if __name__ == "__main__":
    main()
