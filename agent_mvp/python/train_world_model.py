import argparse
import math
import random
from collections import Counter, defaultdict
from itertools import combinations
from pathlib import Path
from typing import Any, DefaultDict, Dict, List, Sequence, Tuple

import numpy as np
import torch
import torch.nn.functional as F
from torch.utils.data import DataLoader

from data_io import read_json, read_jsonl, write_json
from same_initial_signature import canonical_signature_from_record
from world_model import WorldModelNet
from world_model_dataset import (
    EVENT_COUNT_KEYS,
    EVENT_FLAG_KEYS,
    REWARD_KEYS,
    TERMINAL_CRITICAL_ROLE_LABELS,
    WorldModelDataset,
    build_reward_target_tensor,
    collate_world_model,
    derive_effective_termination_flag,
    denormalize_reward_tensor,
    normalize_reward_stats,
)

DEFAULT_BUCKET_KEYS = (
    "tactic_pair_key",
    "red_tactic_id",
    "blue_tactic_id",
    "force_size_key",
    "red_attr_bucket",
    "blue_attr_bucket",
    "terminal_confusion_bucket",
)
SUMMARY_METRIC_KEYS = (
    "trajectory_alive_accuracy",
    "trajectory_missile_mae",
    "trajectory_missile_mse",
    "trajectory_position_error_m",
    "trajectory_alt_mae_m",
    "trajectory_speed_mae_mps",
    "trajectory_heading_mae_deg",
    "terminal_win_accuracy",
    "terminal_red_alive_ratio_mae",
    "terminal_blue_alive_ratio_mae",
    "terminal_red_mean_missile_mae",
    "terminal_blue_mean_missile_mae",
    "consistency_alive_ratio_mae",
    "consistency_missile_mae",
    "consistency_total_mae",
    "event_flag_accuracy",
    "event_first_contact_accuracy",
    "event_first_fire_accuracy",
    "event_warning_accuracy",
    "event_retarget_accuracy",
    "event_first_kill_accuracy",
    "red_first_kill_accuracy",
    "blue_first_kill_accuracy",
    "red_first_kill_positive_recall",
    "blue_first_kill_positive_recall",
    "red_first_kill_precision",
    "blue_first_kill_precision",
    "red_first_kill_high_conf_hit_rate",
    "blue_first_kill_high_conf_hit_rate",
    "blue_first_kill_pred_positive_rate",
    "blue_first_kill_false_positive_rate",
    "blue_first_kill_terminal_false_positive_rate",
    "blue_first_kill_eligible_subset_accuracy",
    "blue_first_kill_eligible_subset_precision",
    "terminal_red_outcome_confusion_rate",
    "terminal_blue_objective_confusion_rate",
    "event_objective_accuracy",
    "event_termination_accuracy",
    "critical_event_accuracy",
    "termination_flag_accuracy",
    "termination_flag_high_conf_hit_rate",
    "first_kill_accuracy",
    "objective_complete_accuracy",
    "event_fire_count_mae",
    "event_kill_count_mae",
    "event_dodge_count_mae",
    "reward_red_mae",
    "reward_blue_mae",
    "reward_total_mae",
    "reward_red_delta_scale",
    "reward_blue_delta_scale",
    "reward_red_true_delta_scale",
    "reward_blue_true_delta_scale",
    "terminal_critical_role_accuracy",
)

MISSILE_SCALE = 8.0
LON_SCALE_DEG = 180.0
LAT_SCALE_DEG = 90.0
ALT_SCALE_M = 20000.0
SPEED_SCALE_MPS = 1000.0
POSITION_LAT_METERS = 111000.0
EVENT_FIRE_COUNT_SCALE = 4.0
REWARD_SCALE = 10.0
COUNTERFACTUAL_METRIC_SPECS = {
    "terminal_red_win_prob": {"weight": 2.0, "signal_threshold": 0.50},
    "trajectory_red_alive_ratio": {"weight": 1.5, "signal_threshold": 0.05},
    "trajectory_red_mean_missile": {"weight": 1.0, "signal_threshold": 0.05},
    "event_red_fire_count": {"weight": 1.5, "signal_threshold": 0.10},
    "event_red_first_kill_prob": {"weight": 1.0, "signal_threshold": 0.50},
    "event_blue_first_kill_prob": {"weight": 1.0, "signal_threshold": 0.50},
    "event_red_objective_complete_prob": {"weight": 1.0, "signal_threshold": 0.50},
    "event_blue_objective_complete_prob": {"weight": 1.0, "signal_threshold": 0.50},
    "termination_flag": {"weight": 2.5, "signal_threshold": 0.20},
    "reward_red": {"weight": 1.5, "signal_threshold": 0.20},
}


EVENT_GROUPS = {
    "contact_warning_group": (
        "red_first_contact_flag",
        "blue_first_contact_flag",
        "red_contact_flag",
        "blue_contact_flag",
        "red_warning_flag",
        "blue_warning_flag",
    ),
    "tactical_transition_group": (
        "red_first_fire_flag",
        "blue_first_fire_flag",
        "red_retarget_flag",
        "blue_retarget_flag",
    ),
    "critical_first_kill_group": (
        "red_first_kill_flag",
        "blue_first_kill_flag",
    ),
    "critical_objective_group": (
        "red_objective_complete_flag",
        "blue_objective_complete_flag",
    ),
    "critical_termination_group": (
        "termination_flag",
    ),
}


RARE_EVENT_POS_WEIGHTS = {
    "termination_flag": 10.0,
    "red_first_kill_flag": 8.0,
    "blue_first_kill_flag": 8.0,
    "red_objective_complete_flag": 7.0,
    "blue_objective_complete_flag": 7.0,
    "red_first_fire_flag": 3.0,
    "blue_first_fire_flag": 3.0,
}


DEFAULT_LOSS_CONFIG = {
    "consistency_weight": 0.2,
    "event_weight": 0.5,
    "reward_weight": 0.25,
    "event_group_weights": {
        "contact_warning_group": 1.0,
        "tactical_transition_group": 1.2,
        "critical_first_kill_group": 2.4,
        "critical_objective_group": 2.2,
        "critical_termination_group": 3.0,
    },
    "termination_extra_weight": 2.5,
    "event_count_weight": 0.4,
    "critical_hard_positive_weight": 1.0,
    "terminal_role_weight": 0.5,
}


STAGE2_LOSS_CONFIG = {
    "consistency_weight": 0.2,
    "event_weight": 1.2,
    "reward_weight": 0.8,
    "event_group_weights": {
        "contact_warning_group": 1.0,
        "tactical_transition_group": 1.4,
        "critical_first_kill_group": 3.8,
        "critical_objective_group": 3.4,
        "critical_termination_group": 4.5,
    },
    "termination_extra_weight": 4.0,
    "event_count_weight": 0.6,
    "critical_hard_positive_weight": 1.8,
    "terminal_role_weight": 0.5,
}


def set_seed(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)


def move_to_device(batch: Dict[str, torch.Tensor], device: torch.device) -> Dict[str, torch.Tensor]:
    moved: Dict[str, torch.Tensor] = {}
    for key, value in batch.items():
        if torch.is_tensor(value):
            moved[key] = value.to(device)
        else:
            moved[key] = value
    return moved


def make_data_loader(
    records: List[Dict],
    batch_size: int,
    shuffle: bool,
    reward_norm_stats: Dict[str, Dict[str, float]],
    target_contract: str = "baseline",
) -> DataLoader:
    dataset = WorldModelDataset(
        records,
        reward_norm_stats=reward_norm_stats,
        target_contract=target_contract,
    )
    return DataLoader(dataset, batch_size=batch_size, shuffle=shuffle, collate_fn=collate_world_model)


def load_split_records(data_dir: Path, split_name: str) -> List[Dict]:
    path = data_dir / "processed" / f"{split_name}.jsonl"
    if not path.exists():
        return []
    records = read_jsonl(path)
    return annotate_first_kill_eligibility(records)


def annotate_first_kill_eligibility(records: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    if not records:
        return records

    terminal_maps: DefaultDict[str, Dict[int, Dict[str, bool]]] = defaultdict(dict)
    episode_any_first_kill: Dict[str, bool] = defaultdict(bool)

    for record in records:
        meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
        episode_id = str(meta.get("episode_id") or record.get("episode_id") or record.get("task_id") or "")
        if not episode_id:
            continue
        if str(record.get("horizon")) != "terminal":
            continue
        t_index = int(meta.get("t_index", 0))
        target_event = record.get("target_event", {}) if isinstance(record.get("target_event", {}), dict) else {}
        future_red_first_kill = bool(target_event.get("red_first_kill_flag", False))
        future_blue_first_kill = bool(target_event.get("blue_first_kill_flag", False))
        future_any_first_kill = future_red_first_kill or future_blue_first_kill
        terminal_maps[episode_id][t_index] = {
            "future_any_first_kill": future_any_first_kill,
            "future_red_first_kill": future_red_first_kill,
            "future_blue_first_kill": future_blue_first_kill,
        }
        episode_any_first_kill[episode_id] = bool(episode_any_first_kill[episode_id] or future_any_first_kill)

    for record in records:
        meta = dict(record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {})
        episode_id = str(meta.get("episode_id") or record.get("episode_id") or record.get("task_id") or "")
        t_index = int(meta.get("t_index", 0))
        horizon = str(record.get("horizon"))
        target_event = record.get("target_event", {}) if isinstance(record.get("target_event", {}), dict) else {}
        future_info = terminal_maps.get(episode_id, {}).get(t_index)
        if future_info is None:
            future_red_first_kill = bool(target_event.get("red_first_kill_flag", False))
            future_blue_first_kill = bool(target_event.get("blue_first_kill_flag", False))
            future_any_first_kill = future_red_first_kill or future_blue_first_kill
        else:
            future_red_first_kill = future_info["future_red_first_kill"]
            future_blue_first_kill = future_info["future_blue_first_kill"]
            future_any_first_kill = future_info["future_any_first_kill"]

        any_first_kill_in_episode = bool(episode_any_first_kill.get(episode_id, future_any_first_kill))
        prior_kill_seen = bool(any_first_kill_in_episode and not future_any_first_kill)
        red_first_kill_true = bool(target_event.get("red_first_kill_flag", False))
        blue_first_kill_true = bool(target_event.get("blue_first_kill_flag", False))
        red_objective_true = bool(target_event.get("red_objective_complete_flag", False))
        blue_objective_true = bool(target_event.get("blue_objective_complete_flag", False))
        target_termination_reason = str(meta.get("target_termination_reason", "")).strip().lower()
        effective_terminal = bool(
            horizon == "terminal"
            and target_termination_reason
            and target_termination_reason not in {"none", "safety_limit", "timeout", "time_limit"}
        )
        terminal_red_outcome_conflict = bool(
            effective_terminal
            and (not blue_first_kill_true)
            and red_first_kill_true
            and red_objective_true
        )
        terminal_blue_objective_conflict = bool(
            effective_terminal
            and (not blue_first_kill_true)
            and (not red_first_kill_true)
            and blue_objective_true
        )
        terminal_no_first_kill_remaining_conflict = bool(
            horizon == "terminal"
            and (not blue_first_kill_true)
            and (not red_first_kill_true)
            and (not future_any_first_kill)
            and (not effective_terminal)
            and (not red_objective_true)
            and (not blue_objective_true)
        )
        blue_first_kill_terminal_conflict = bool(
            terminal_red_outcome_conflict
            or terminal_blue_objective_conflict
            or terminal_no_first_kill_remaining_conflict
        )
        terminal_confusion_bucket = "none"
        if terminal_red_outcome_conflict:
            terminal_confusion_bucket = "terminal_red_first_kill_outcome_confusion"
        elif terminal_blue_objective_conflict:
            terminal_confusion_bucket = "terminal_blue_objective_without_first_kill"
        elif terminal_no_first_kill_remaining_conflict:
            terminal_confusion_bucket = "terminal_or_late_window_without_any_first_kill_remaining"
        meta["prior_kill_seen"] = prior_kill_seen
        meta["red_first_kill_eligible"] = bool(future_red_first_kill)
        meta["blue_first_kill_eligible"] = bool(future_blue_first_kill)
        meta["future_red_first_kill_possible"] = bool(future_red_first_kill)
        meta["future_blue_first_kill_possible"] = bool(future_blue_first_kill)
        meta["effective_terminal"] = effective_terminal
        meta["terminal_red_outcome_conflict"] = terminal_red_outcome_conflict
        meta["terminal_blue_objective_without_blue_first_kill_conflict"] = terminal_blue_objective_conflict
        meta["terminal_no_first_kill_remaining_conflict"] = terminal_no_first_kill_remaining_conflict
        meta["blue_first_kill_terminal_conflict"] = blue_first_kill_terminal_conflict
        meta["blue_first_kill_terminal_eligible"] = bool(
            horizon != "terminal" or blue_first_kill_true or (not blue_first_kill_terminal_conflict)
        )
        meta["terminal_confusion_bucket"] = terminal_confusion_bucket
        record["meta"] = meta

    return records


def compute_reward_norm_stats(records: Sequence[Dict[str, Any]]) -> Dict[str, Dict[str, float]]:
    stats: Dict[str, Dict[str, float]] = {}
    for side in REWARD_KEYS:
        values: List[float] = []
        for record in records:
            reward = record.get("target_reward", {})
            if isinstance(reward, dict):
                values.append(float(reward.get(side, 0.0)))
        if not values:
            stats[side] = {
                "clip_low": -10.0,
                "clip_high": 10.0,
                "mean": 0.0,
                "std": 1.0,
            }
            continue

        arr = np.asarray(values, dtype=np.float64)
        clip_low = float(np.percentile(arr, 1.0))
        clip_high = float(np.percentile(arr, 99.0))
        if clip_high < clip_low:
            clip_low, clip_high = clip_high, clip_low
        clipped = np.clip(arr, clip_low, clip_high)
        std = float(np.std(clipped))
        stats[side] = {
            "clip_low": clip_low,
            "clip_high": clip_high,
            "mean": float(np.mean(clipped)),
            "std": max(std, 0.1),
        }
    return normalize_reward_stats(stats)


def _event_flag_indices(keys: Sequence[str]) -> List[int]:
    index_map = {key: idx for idx, key in enumerate(EVENT_FLAG_KEYS)}
    return [index_map[key] for key in keys if key in index_map]


def _build_loss_config(overrides: Dict[str, Any] = None) -> Dict[str, Any]:
    output = {
        "consistency_weight": float(DEFAULT_LOSS_CONFIG["consistency_weight"]),
        "event_weight": float(DEFAULT_LOSS_CONFIG["event_weight"]),
        "reward_weight": float(DEFAULT_LOSS_CONFIG["reward_weight"]),
        "event_group_weights": dict(DEFAULT_LOSS_CONFIG["event_group_weights"]),
        "termination_extra_weight": float(DEFAULT_LOSS_CONFIG["termination_extra_weight"]),
        "event_count_weight": float(DEFAULT_LOSS_CONFIG["event_count_weight"]),
        "critical_hard_positive_weight": float(DEFAULT_LOSS_CONFIG["critical_hard_positive_weight"]),
        "terminal_role_weight": float(DEFAULT_LOSS_CONFIG["terminal_role_weight"]),
    }
    if isinstance(overrides, dict):
        for key, value in overrides.items():
            if key == "event_group_weights" and isinstance(value, dict):
                output[key].update({k: float(v) for k, v in value.items()})
            elif key in output:
                output[key] = float(value) if isinstance(output[key], float) else value
    return output


def _compute_event_grouped_loss(
    event_flag_logits: torch.Tensor,
    event_flags: torch.Tensor,
    event_flag_weights: torch.Tensor | None,
    loss_config: Dict[str, Any],
) -> Tuple[torch.Tensor, Dict[str, torch.Tensor]]:
    device = event_flag_logits.device
    pos_weight = torch.ones((len(EVENT_FLAG_KEYS),), dtype=torch.float32, device=device)
    for key, weight in RARE_EVENT_POS_WEIGHTS.items():
        if key in EVENT_FLAG_KEYS:
            pos_weight[EVENT_FLAG_KEYS.index(key)] = float(weight)

    bce_raw = F.binary_cross_entropy_with_logits(
        event_flag_logits,
        event_flags,
        pos_weight=pos_weight,
        reduction="none",
    )
    if event_flag_weights is None:
        event_flag_weights = torch.ones_like(bce_raw)
    else:
        event_flag_weights = event_flag_weights.to(device=device, dtype=bce_raw.dtype)

    group_losses: Dict[str, torch.Tensor] = {}
    weighted_sum = torch.zeros((), device=device)
    weight_total = 0.0
    for group_name, keys in EVENT_GROUPS.items():
        indices = _event_flag_indices(keys)
        if not indices:
            continue
        group_weight_tensor = event_flag_weights[:, indices]
        group_loss = (bce_raw[:, indices] * group_weight_tensor).sum() / group_weight_tensor.sum().clamp(min=1.0)
        group_losses[group_name] = group_loss
        group_weight = float(loss_config["event_group_weights"].get(group_name, 1.0))
        weighted_sum = weighted_sum + group_weight * group_loss
        weight_total += group_weight

    termination_idx = EVENT_FLAG_KEYS.index("termination_flag")
    termination_weights = event_flag_weights[:, termination_idx]
    termination_loss = (bce_raw[:, termination_idx] * termination_weights).sum() / termination_weights.sum().clamp(min=1.0)
    group_losses["termination_flag"] = termination_loss

    critical_indices = _event_flag_indices(
        (
            "red_first_kill_flag",
            "blue_first_kill_flag",
            "red_objective_complete_flag",
            "blue_objective_complete_flag",
            "termination_flag",
        )
    )
    if critical_indices:
        critical_logits = event_flag_logits[:, critical_indices]
        critical_targets = event_flags[:, critical_indices]
        critical_probs = torch.sigmoid(critical_logits)
        critical_bce = bce_raw[:, critical_indices]
        hard_positive_mask = (critical_targets >= 0.5).float()
        hard_positive_factor = ((1.0 - critical_probs) ** 2) * hard_positive_mask
        hard_positive_denom = hard_positive_mask.sum().clamp(min=1.0)
        critical_hard_positive_loss = (critical_bce * hard_positive_factor).sum() / hard_positive_denom
    else:
        critical_hard_positive_loss = torch.zeros((), device=device)
    group_losses["critical_hard_positive"] = critical_hard_positive_loss

    if weight_total <= 0.0:
        event_flag_loss = bce_raw.mean()
    else:
        event_flag_loss = weighted_sum / weight_total
    event_flag_loss = event_flag_loss + float(loss_config["termination_extra_weight"]) * termination_loss
    event_flag_loss = event_flag_loss + float(loss_config["critical_hard_positive_weight"]) * critical_hard_positive_loss

    return event_flag_loss, group_losses


def _state_signature_from_record(record: Dict[str, Any]) -> str:
    return canonical_signature_from_record(record)


def _safe_mean_raw(values: Sequence[float]) -> float:
    if not values:
        return 0.0
    return float(sum(values) / len(values))


def _extract_counterfactual_targets(record: Dict[str, Any]) -> Dict[str, float]:
    target_state = record.get("target_state_t_plus_h", {})
    red_units = target_state.get("red_units", [])
    target_event = record.get("target_event", {})
    target_reward = record.get("target_reward", {})
    outcome = record.get("target_terminal", {}).get("episode_outcome", {})

    red_alive_ratio = _safe_mean_raw([float(unit.get("alive", 0.0)) for unit in red_units])
    red_mean_missile = _safe_mean_raw(
        [float(unit.get("missile_count", 0.0)) / MISSILE_SCALE for unit in red_units]
    )
    red_fire_count = float(target_event.get("red_fire_count_delta", 0.0)) / EVENT_FIRE_COUNT_SCALE
    termination_flag = float(derive_effective_termination_flag(record))
    reward_red = float(target_reward.get("red", 0.0)) / REWARD_SCALE
    red_first_kill = 1.0 if bool(target_event.get("red_first_kill_flag", False)) else 0.0
    blue_first_kill = 1.0 if bool(target_event.get("blue_first_kill_flag", False)) else 0.0
    red_objective = 1.0 if bool(target_event.get("red_objective_complete_flag", False)) else 0.0
    blue_objective = 1.0 if bool(target_event.get("blue_objective_complete_flag", False)) else 0.0

    return {
        "terminal_red_win_prob": float(outcome.get("red_win", 0.0)),
        "trajectory_red_alive_ratio": red_alive_ratio,
        "trajectory_red_mean_missile": red_mean_missile,
        "event_red_fire_count": red_fire_count,
        "event_red_first_kill_prob": red_first_kill,
        "event_blue_first_kill_prob": blue_first_kill,
        "event_red_objective_complete_prob": red_objective,
        "event_blue_objective_complete_prob": blue_objective,
        "termination_flag": termination_flag,
        "reward_red": reward_red,
    }


def build_counterfactual_pairs(
    records: Sequence[Dict[str, Any]],
    horizons: Sequence[str],
    state_step: int,
    min_signal: float,
    include_reverse: bool = True,
) -> List[Dict[str, Any]]:
    requested_horizons = {str(horizon) for horizon in horizons}
    grouped: DefaultDict[Tuple[str, str, int], List[Dict[str, Any]]] = defaultdict(list)

    for record in records:
        meta = record.get("meta", {})
        record_state_step = int(meta.get("state_step", -1))
        if record_state_step != state_step:
            continue
        if requested_horizons and str(record.get("horizon")) not in requested_horizons:
            continue

        sampling_meta = meta.get("sampling_meta", {}) if isinstance(meta.get("sampling_meta", {}), dict) else {}
        tactic_combo_key = sampling_meta.get("tactic_combo_key")
        if tactic_combo_key is None:
            red_cond = record.get("red_tactic_condition", {})
            blue_cond = record.get("blue_tactic_condition", {})
            tactic_combo_key = f"{red_cond.get('id', 'red_unknown')}__{blue_cond.get('id', 'blue_unknown')}"

        grouped[(_state_signature_from_record(record), str(record.get("horizon")), record_state_step)].append(
            {
                "record": record,
                "tactic_combo_key": str(tactic_combo_key),
                "targets": _extract_counterfactual_targets(record),
            }
        )

    pair_entries: List[Dict[str, Any]] = []
    for group_key, group_items in grouped.items():
        unique_combos = {item["tactic_combo_key"] for item in group_items}
        if len(group_items) < 2 or len(unique_combos) < 2:
            continue

        for left_item, right_item in combinations(group_items, 2):
            if left_item["tactic_combo_key"] == right_item["tactic_combo_key"]:
                continue

            delta_targets = {
                metric_name: float(right_item["targets"][metric_name] - left_item["targets"][metric_name])
                for metric_name in COUNTERFACTUAL_METRIC_SPECS
            }
            signal_strength = sum(
                abs(delta_targets[metric_name]) * metric_spec["weight"]
                for metric_name, metric_spec in COUNTERFACTUAL_METRIC_SPECS.items()
            )
            signal_pairs = sum(
                1
                for metric_name, metric_spec in COUNTERFACTUAL_METRIC_SPECS.items()
                if abs(delta_targets[metric_name]) >= float(metric_spec["signal_threshold"])
            )
            if signal_strength < min_signal:
                continue

            base_entry = {
                "group_key": group_key,
                "state_signature": group_key[0],
                "horizon": group_key[1],
                "state_step": group_key[2],
                "left_record": left_item["record"],
                "right_record": right_item["record"],
                "left_tactic_combo_key": left_item["tactic_combo_key"],
                "right_tactic_combo_key": right_item["tactic_combo_key"],
                "delta_targets": delta_targets,
                "signal_strength": signal_strength,
                "signal_pairs": signal_pairs,
                "pair_weight": 1.0 + signal_strength,
            }
            pair_entries.append(base_entry)
            if include_reverse:
                pair_entries.append(
                    {
                        **base_entry,
                        "left_record": right_item["record"],
                        "right_record": left_item["record"],
                        "left_tactic_combo_key": right_item["tactic_combo_key"],
                        "right_tactic_combo_key": left_item["tactic_combo_key"],
                        "delta_targets": {
                            metric_name: -delta_value
                            for metric_name, delta_value in delta_targets.items()
                        },
                    }
                )

    return pair_entries


def _records_to_batch(
    records: Sequence[Dict[str, Any]],
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
) -> Dict[str, Any]:
    dataset = WorldModelDataset(list(records), reward_norm_stats=reward_norm_stats)
    batch = collate_world_model([dataset[index] for index in range(len(dataset))])
    return move_to_device(batch, device)


def _extract_counterfactual_predictions(outputs: Dict[str, torch.Tensor], batch: Dict[str, torch.Tensor]) -> Dict[str, torch.Tensor]:
    red_fire_index = EVENT_COUNT_KEYS.index("red_fire_count_delta")
    termination_index = EVENT_FLAG_KEYS.index("termination_flag")
    red_first_kill_index = EVENT_FLAG_KEYS.index("red_first_kill_flag")
    blue_first_kill_index = EVENT_FLAG_KEYS.index("blue_first_kill_flag")
    red_objective_index = EVENT_FLAG_KEYS.index("red_objective_complete_flag")
    blue_objective_index = EVENT_FLAG_KEYS.index("blue_objective_complete_flag")
    return {
        "terminal_red_win_prob": torch.sigmoid(outputs["red_win_logit"]),
        "trajectory_red_alive_ratio": _masked_mean_scalar(torch.sigmoid(outputs["red_alive_logit"]), batch["red_mask"]),
        "trajectory_red_mean_missile": _masked_mean_scalar(outputs["red_traj_reg"][..., 0], batch["red_mask"]),
        "event_red_fire_count": torch.clamp(outputs["event_count_pred"][:, red_fire_index], min=0.0) / EVENT_FIRE_COUNT_SCALE,
        "event_red_first_kill_prob": torch.sigmoid(outputs["event_flag_logits"][:, red_first_kill_index]),
        "event_blue_first_kill_prob": torch.sigmoid(outputs["event_flag_logits"][:, blue_first_kill_index]),
        "event_red_objective_complete_prob": torch.sigmoid(outputs["event_flag_logits"][:, red_objective_index]),
        "event_blue_objective_complete_prob": torch.sigmoid(outputs["event_flag_logits"][:, blue_objective_index]),
        "termination_flag": torch.sigmoid(outputs["event_flag_logits"][:, termination_index]),
        "reward_red": outputs["reward_pred"][:, 0] / REWARD_SCALE,
    }


def compute_counterfactual_pair_loss(
    model: WorldModelNet,
    pair_entries: Sequence[Dict[str, Any]],
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
    loss_config: Dict[str, Any] = None,
) -> Dict[str, Any]:
    left_batch = _records_to_batch(
        [entry["left_record"] for entry in pair_entries],
        device,
        reward_norm_stats=reward_norm_stats,
    )
    right_batch = _records_to_batch(
        [entry["right_record"] for entry in pair_entries],
        device,
        reward_norm_stats=reward_norm_stats,
    )

    left_outputs, left_losses = compute_losses(model, left_batch, loss_config=loss_config)
    right_outputs, right_losses = compute_losses(model, right_batch, loss_config=loss_config)
    left_preds = _extract_counterfactual_predictions(left_outputs, left_batch)
    right_preds = _extract_counterfactual_predictions(right_outputs, right_batch)

    pair_weights = torch.tensor(
        [float(entry["pair_weight"]) for entry in pair_entries],
        dtype=torch.float32,
        device=device,
    )
    weight_denom = pair_weights.sum().clamp(min=1.0)

    delta_loss_terms: Dict[str, torch.Tensor] = {}
    mean_pred_abs_delta: Dict[str, float] = {}
    mean_true_abs_delta: Dict[str, float] = {}
    total_delta_loss = torch.zeros((), device=device)

    for metric_name, metric_spec in COUNTERFACTUAL_METRIC_SPECS.items():
        pred_delta = right_preds[metric_name] - left_preds[metric_name]
        true_delta = torch.tensor(
            [float(entry["delta_targets"][metric_name]) for entry in pair_entries],
            dtype=torch.float32,
            device=device,
        )
        per_pair_loss = F.smooth_l1_loss(pred_delta, true_delta, reduction="none")
        metric_loss = (per_pair_loss * pair_weights).sum() / weight_denom
        total_delta_loss = total_delta_loss + float(metric_spec["weight"]) * metric_loss
        delta_loss_terms[metric_name] = metric_loss
        mean_pred_abs_delta[metric_name] = float(torch.abs(pred_delta).mean().item())
        mean_true_abs_delta[metric_name] = float(torch.abs(true_delta).mean().item())

    sample_loss = 0.5 * (left_losses["total"] + right_losses["total"])
    return {
        "sample_loss": sample_loss,
        "delta_loss": total_delta_loss,
        "metric_losses": {key: float(value.item()) for key, value in delta_loss_terms.items()},
        "mean_pred_abs_delta": mean_pred_abs_delta,
        "mean_true_abs_delta": mean_true_abs_delta,
        "num_pairs": len(pair_entries),
    }

def _masked_mean_scalar(values: torch.Tensor, mask: torch.Tensor) -> torch.Tensor:
    weights = mask.float()
    denom = weights.sum(dim=1).clamp(min=1.0)
    return (values * weights).sum(dim=1) / denom


def _mean_or_zero(values: torch.Tensor, mask: torch.Tensor) -> float:
    if int(mask.sum().item()) <= 0:
        return 0.0
    return float(values[mask].mean().item())


def _binary_accuracy(pred_probs: torch.Tensor, targets: torch.Tensor, indices: Sequence[int]) -> float:
    if not indices:
        return 0.0
    pred = (pred_probs[:, list(indices)] >= 0.5).float()
    target = targets[:, list(indices)]
    return float((pred == target).float().mean().item())


def _binary_single_accuracy(pred_prob: float, target_value: float, threshold: float = 0.5) -> float:
    pred_value = 1.0 if pred_prob >= threshold else 0.0
    return 1.0 if pred_value == (1.0 if target_value >= 0.5 else 0.0) else 0.0


def _positive_recall_value(pred_prob: float, target_value: float, threshold: float = 0.5) -> Any:
    if target_value < 0.5:
        return None
    return 1.0 if pred_prob >= threshold else 0.0


def _precision_value(pred_prob: float, target_value: float, threshold: float = 0.5) -> Any:
    if pred_prob < threshold:
        return None
    return 1.0 if target_value >= 0.5 else 0.0


def _high_conf_hit_value(pred_prob: float, target_value: float, high_conf_threshold: float = 0.7) -> Any:
    if target_value < 0.5:
        return None
    return 1.0 if pred_prob >= high_conf_threshold else 0.0


def _pred_positive_value(pred_prob: float, threshold: float = 0.5) -> float:
    return 1.0 if pred_prob >= threshold else 0.0


def _false_positive_value(pred_prob: float, target_value: float, threshold: float = 0.5) -> Any:
    if target_value >= 0.5:
        return None
    return 1.0 if pred_prob >= threshold else 0.0


def _normalize_split_value(value: Any) -> str:
    if value is None:
        return "unknown"
    if isinstance(value, (int, float)):
        if isinstance(value, float) and value.is_integer():
            return str(int(value))
        return str(value)
    text = str(value).strip()
    return text or "unknown"


def _extract_bucket_value(record: Dict[str, Any], bucket_key: str) -> str:
    value = record.get(bucket_key)
    if value is None:
        meta = record.get("meta", {})
        if isinstance(meta, dict):
            value = meta.get(bucket_key)
            if value is None:
                sampling_meta = meta.get("sampling_meta", {})
                if isinstance(sampling_meta, dict):
                    value = sampling_meta.get(bucket_key)
    if value is None:
        return "unknown"
    return _normalize_split_value(value)


def _record_first_kill_flags(record: Dict[str, Any]) -> Tuple[bool, bool]:
    target_event = record.get("target_event", {})
    if not isinstance(target_event, dict):
        target_event = {}
    return (
        bool(target_event.get("red_first_kill_flag", False)),
        bool(target_event.get("blue_first_kill_flag", False)),
    )


def _record_first_kill_hard_negative(record: Dict[str, Any]) -> bool:
    target_event = record.get("target_event", {})
    if not isinstance(target_event, dict):
        target_event = {}
    red_first_kill, blue_first_kill = _record_first_kill_flags(record)
    if red_first_kill or blue_first_kill:
        return False
    return bool(
        float(target_event.get("red_kill_delta", 0.0)) > 0.0
        or float(target_event.get("blue_kill_delta", 0.0)) > 0.0
        or bool(target_event.get("termination_flag", False))
        or bool(target_event.get("red_objective_complete_flag", False))
        or bool(target_event.get("blue_objective_complete_flag", False))
    )


def _record_side_first_kill_hard_negative(record: Dict[str, Any], side: str) -> bool:
    target_event = record.get("target_event", {})
    if not isinstance(target_event, dict):
        target_event = {}
    red_first_kill, blue_first_kill = _record_first_kill_flags(record)
    if side == "red":
        if red_first_kill:
            return False
        return bool(
            float(target_event.get("red_kill_delta", 0.0)) > 0.0
            or bool(target_event.get("termination_flag", False))
            or bool(target_event.get("red_objective_complete_flag", False))
            or blue_first_kill
        )
    if side == "blue":
        if blue_first_kill:
            return False
        return bool(
            float(target_event.get("blue_kill_delta", 0.0)) > 0.0
            or bool(target_event.get("termination_flag", False))
            or bool(target_event.get("blue_objective_complete_flag", False))
            or red_first_kill
        )
    raise ValueError(f"Unsupported side: {side}")


def _record_side_first_kill_eligible(record: Dict[str, Any], side: str) -> bool:
    meta = record.get("meta", {})
    if not isinstance(meta, dict):
        meta = {}
    default_value = not bool(meta.get("prior_kill_seen", False))
    return bool(meta.get(f"{side}_first_kill_eligible", default_value))


def _record_blue_terminal_conflict(record: Dict[str, Any]) -> bool:
    meta = record.get("meta", {})
    if not isinstance(meta, dict):
        return False
    return bool(meta.get("blue_first_kill_terminal_conflict", False))


def build_first_kill_stage2_records(
    records: Sequence[Dict[str, Any]],
    positive_repeat: int,
    hard_negative_repeat: int,
    red_positive_repeat: int = None,
    blue_positive_repeat: int = None,
    red_hard_negative_repeat: int = None,
    blue_hard_negative_repeat: int = None,
    blue_terminal_conflict_repeat: int = 1,
) -> Tuple[List[Dict[str, Any]], Dict[str, Any]]:
    output: List[Dict[str, Any]] = []
    positive_count = 0
    hard_negative_count = 0
    red_positive_count = 0
    blue_positive_count = 0
    red_hard_negative_count = 0
    blue_hard_negative_count = 0
    blue_terminal_conflict_count = 0
    blue_eligible_count = 0
    blue_ineligible_filtered_count = 0
    base_count = len(records)
    red_positive_repeat = int(red_positive_repeat if red_positive_repeat is not None else positive_repeat)
    blue_positive_repeat = int(blue_positive_repeat if blue_positive_repeat is not None else positive_repeat)
    red_hard_negative_repeat = int(
        red_hard_negative_repeat if red_hard_negative_repeat is not None else hard_negative_repeat
    )
    blue_hard_negative_repeat = int(
        blue_hard_negative_repeat if blue_hard_negative_repeat is not None else hard_negative_repeat
    )
    for record in records:
        repeats = 1
        red_first_kill, blue_first_kill = _record_first_kill_flags(record)
        blue_eligible = _record_side_first_kill_eligible(record, "blue")
        if blue_eligible:
            blue_eligible_count += 1
        if red_first_kill or blue_first_kill:
            if red_first_kill:
                repeats = max(repeats, red_positive_repeat)
                red_positive_count += 1
            if blue_first_kill and blue_eligible:
                repeats = max(repeats, blue_positive_repeat)
                blue_positive_count += 1
            elif blue_first_kill and not blue_eligible:
                blue_ineligible_filtered_count += 1
            positive_count += 1
        else:
            red_hard_negative = _record_side_first_kill_hard_negative(record, "red")
            raw_blue_hard_negative = _record_side_first_kill_hard_negative(record, "blue")
            blue_hard_negative = blue_eligible and raw_blue_hard_negative
            blue_terminal_conflict = _record_blue_terminal_conflict(record)
            if red_hard_negative:
                repeats = max(repeats, red_hard_negative_repeat)
                red_hard_negative_count += 1
            if blue_hard_negative:
                repeats = max(repeats, blue_hard_negative_repeat)
                blue_hard_negative_count += 1
            elif raw_blue_hard_negative and not blue_eligible:
                blue_ineligible_filtered_count += 1
            if blue_terminal_conflict:
                repeats = max(repeats, int(blue_terminal_conflict_repeat))
                blue_terminal_conflict_count += 1
            if red_hard_negative or blue_hard_negative:
                hard_negative_count += 1
        for _ in range(max(repeats, 1)):
            output.append(record)
    return output, {
        "base_count": base_count,
        "expanded_count": len(output),
        "first_kill_positive_count": positive_count,
        "first_kill_hard_negative_count": hard_negative_count,
        "positive_repeat": int(positive_repeat),
        "hard_negative_repeat": int(hard_negative_repeat),
        "red_first_kill_positive_count": red_positive_count,
        "blue_first_kill_positive_count": blue_positive_count,
        "red_first_kill_hard_negative_count": red_hard_negative_count,
        "blue_first_kill_hard_negative_count": blue_hard_negative_count,
        "blue_terminal_conflict_count": blue_terminal_conflict_count,
        "blue_eligible_count": blue_eligible_count,
        "blue_ineligible_filtered_count": blue_ineligible_filtered_count,
        "red_positive_repeat": red_positive_repeat,
        "blue_positive_repeat": blue_positive_repeat,
        "red_hard_negative_repeat": red_hard_negative_repeat,
        "blue_hard_negative_repeat": blue_hard_negative_repeat,
        "blue_terminal_conflict_repeat": int(blue_terminal_conflict_repeat),
    }


def _heading_norm_to_deg(values: torch.Tensor) -> torch.Tensor:
    return torch.remainder((values + 1.0) * 180.0, 360.0)


def _circular_abs_diff_deg(pred_deg: torch.Tensor, target_deg: torch.Tensor) -> torch.Tensor:
    diff = torch.abs(pred_deg - target_deg)
    return torch.minimum(diff, 360.0 - diff)


def _denormalize_lon_deg(values: torch.Tensor) -> torch.Tensor:
    return values * LON_SCALE_DEG


def _denormalize_lat_deg(values: torch.Tensor) -> torch.Tensor:
    return values * LAT_SCALE_DEG


def _denormalize_alt_m(values: torch.Tensor) -> torch.Tensor:
    return values * ALT_SCALE_M


def _denormalize_speed_mps(values: torch.Tensor) -> torch.Tensor:
    return values * SPEED_SCALE_MPS


def _denormalize_missile(values: torch.Tensor) -> torch.Tensor:
    return values * MISSILE_SCALE


def _position_error_m(
    pred_lon_norm: torch.Tensor,
    pred_lat_norm: torch.Tensor,
    target_lon_norm: torch.Tensor,
    target_lat_norm: torch.Tensor,
) -> torch.Tensor:
    pred_lon_deg = _denormalize_lon_deg(pred_lon_norm)
    pred_lat_deg = _denormalize_lat_deg(pred_lat_norm)
    target_lon_deg = _denormalize_lon_deg(target_lon_norm)
    target_lat_deg = _denormalize_lat_deg(target_lat_norm)

    mean_lat_rad = torch.deg2rad((pred_lat_deg + target_lat_deg) * 0.5)
    dx_m = (pred_lon_deg - target_lon_deg) * POSITION_LAT_METERS * torch.cos(mean_lat_rad)
    dy_m = (pred_lat_deg - target_lat_deg) * POSITION_LAT_METERS
    return torch.sqrt(dx_m * dx_m + dy_m * dy_m)


def compute_losses(
    model: WorldModelNet,
    batch: Dict[str, torch.Tensor],
    loss_config: Dict[str, Any] = None,
) -> Tuple[Dict[str, torch.Tensor], Dict[str, torch.Tensor]]:
    cfg = _build_loss_config(loss_config)
    outputs = model(batch)

    red_valid = batch["red_mask"].float()
    blue_valid = batch["blue_mask"].float()

    red_alive_target = batch["red_target"][..., 0]
    blue_alive_target = batch["blue_target"][..., 0]
    red_reg_target = batch["red_target"][..., 1:]
    blue_reg_target = batch["blue_target"][..., 1:]

    red_alive_loss = (
        F.binary_cross_entropy_with_logits(outputs["red_alive_logit"], red_alive_target, reduction="none") * red_valid
    ).sum() / red_valid.sum().clamp(min=1.0)
    blue_alive_loss = (
        F.binary_cross_entropy_with_logits(outputs["blue_alive_logit"], blue_alive_target, reduction="none") * blue_valid
    ).sum() / blue_valid.sum().clamp(min=1.0)

    red_reg_loss = (
        ((outputs["red_traj_reg"] - red_reg_target) ** 2) * red_valid.unsqueeze(-1)
    ).sum() / (red_valid.sum().clamp(min=1.0) * red_reg_target.shape[-1])
    blue_reg_loss = (
        ((outputs["blue_traj_reg"] - blue_reg_target) ** 2) * blue_valid.unsqueeze(-1)
    ).sum() / (blue_valid.sum().clamp(min=1.0) * blue_reg_target.shape[-1])

    trajectory_loss = red_alive_loss + blue_alive_loss + red_reg_loss + blue_reg_loss

    terminal_win_loss = F.binary_cross_entropy_with_logits(outputs["red_win_logit"], batch["terminal_red_win"])
    terminal_scalar_loss = (
        F.mse_loss(outputs["terminal_red_alive_ratio"], batch["terminal_red_alive_ratio"])
        + F.mse_loss(outputs["terminal_blue_alive_ratio"], batch["terminal_blue_alive_ratio"])
        + F.mse_loss(outputs["terminal_red_mean_missile"], batch["terminal_red_mean_missile"])
        + F.mse_loss(outputs["terminal_blue_mean_missile"], batch["terminal_blue_mean_missile"])
    )
    terminal_loss = terminal_win_loss + terminal_scalar_loss

    is_terminal = batch["is_terminal_horizon"]
    if float(is_terminal.sum().item()) > 0.0:
        pred_red_alive_ratio = _masked_mean_scalar(torch.sigmoid(outputs["red_alive_logit"]), batch["red_mask"])
        pred_blue_alive_ratio = _masked_mean_scalar(torch.sigmoid(outputs["blue_alive_logit"]), batch["blue_mask"])
        pred_red_mean_missile = _masked_mean_scalar(outputs["red_traj_reg"][..., 0], batch["red_mask"])
        pred_blue_mean_missile = _masked_mean_scalar(outputs["blue_traj_reg"][..., 0], batch["blue_mask"])

        consistency_per_sample = (
            (pred_red_alive_ratio - outputs["terminal_red_alive_ratio"]) ** 2
            + (pred_blue_alive_ratio - outputs["terminal_blue_alive_ratio"]) ** 2
            + (pred_red_mean_missile - outputs["terminal_red_mean_missile"]) ** 2
            + (pred_blue_mean_missile - outputs["terminal_blue_mean_missile"]) ** 2
        )
        consistency_loss = (consistency_per_sample * is_terminal).sum() / is_terminal.sum().clamp(min=1.0)
    else:
        consistency_loss = torch.zeros((), device=batch["terminal_red_win"].device)

    event_flag_loss, event_group_losses = _compute_event_grouped_loss(
        outputs["event_flag_logits"],
        batch["event_flags"],
        batch.get("event_flag_weights"),
        cfg,
    )
    event_count_loss = F.smooth_l1_loss(
        outputs["event_count_pred"],
        batch["event_counts"],
    )
    event_loss = event_flag_loss + float(cfg["event_count_weight"]) * event_count_loss
    reward_loss = F.smooth_l1_loss(outputs["reward_pred"], batch["reward_target"])
    role_mask = batch["terminal_critical_role_mask"]
    if float(role_mask.sum().item()) > 0.0:
        role_losses = F.cross_entropy(
            outputs["terminal_critical_role_logits"],
            batch["terminal_critical_role_id"],
            reduction="none",
        )
        terminal_role_loss = (role_losses * role_mask).sum() / role_mask.sum().clamp(min=1.0)
    else:
        terminal_role_loss = torch.zeros((), device=batch["terminal_red_win"].device)

    total_loss = (
        trajectory_loss
        + terminal_loss
        + float(cfg["consistency_weight"]) * consistency_loss
        + float(cfg["event_weight"]) * event_loss
        + float(cfg["reward_weight"]) * reward_loss
        + float(cfg["terminal_role_weight"]) * terminal_role_loss
    )
    return outputs, {
        "total": total_loss,
        "trajectory": trajectory_loss,
        "terminal": terminal_loss,
        "consistency": consistency_loss,
        "event": event_loss,
        "event_flag": event_flag_loss,
        "event_count": event_count_loss,
        "event_contact_warning": event_group_losses.get("contact_warning_group", torch.zeros_like(event_flag_loss)),
        "event_tactical_transition": event_group_losses.get("tactical_transition_group", torch.zeros_like(event_flag_loss)),
        "event_critical_outcome": (
            event_group_losses.get("critical_first_kill_group", torch.zeros_like(event_flag_loss))
            + event_group_losses.get("critical_objective_group", torch.zeros_like(event_flag_loss))
            + event_group_losses.get("critical_termination_group", torch.zeros_like(event_flag_loss))
        ) / 3.0,
        "event_termination": event_group_losses.get("termination_flag", torch.zeros_like(event_flag_loss)),
        "event_critical_hard_positive": event_group_losses.get("critical_hard_positive", torch.zeros_like(event_flag_loss)),
        "reward": reward_loss,
        "terminal_role": terminal_role_loss,
    }


def _team_sample_summary(
    alive_logit: torch.Tensor,
    reg_pred: torch.Tensor,
    target_features: torch.Tensor,
    state_mask: torch.Tensor,
    target_mask: torch.Tensor,
) -> Dict[str, float]:
    valid = state_mask.bool() & target_mask.bool()
    count = int(valid.sum().item())
    if count <= 0:
        return {
            "count": 0,
            "alive_correct": 0.0,
            "missile_abs_sum": 0.0,
            "missile_sq_sum": 0.0,
            "position_error_sum": 0.0,
            "alt_abs_sum": 0.0,
            "speed_abs_sum": 0.0,
            "heading_abs_sum": 0.0,
            "pred_alive_ratio": 0.0,
            "true_alive_ratio": 0.0,
            "pred_mean_missile": 0.0,
            "true_mean_missile": 0.0,
        }

    target_alive = target_features[..., 0]
    target_missile = target_features[..., 1]
    target_lon = target_features[..., 2]
    target_lat = target_features[..., 3]
    target_alt = target_features[..., 4]
    target_speed = target_features[..., 5]
    target_heading = target_features[..., 6]

    pred_alive_prob = torch.sigmoid(alive_logit)
    pred_alive_binary = (pred_alive_prob >= 0.5).float()
    alive_correct = (pred_alive_binary == (target_alive >= 0.5).float()).float()

    pred_missile = _denormalize_missile(reg_pred[..., 0])
    true_missile = _denormalize_missile(target_missile)
    missile_abs = torch.abs(pred_missile - true_missile)
    missile_sq = (pred_missile - true_missile) ** 2

    position_error = _position_error_m(reg_pred[..., 1], reg_pred[..., 2], target_lon, target_lat)
    alt_abs = torch.abs(_denormalize_alt_m(reg_pred[..., 3]) - _denormalize_alt_m(target_alt))
    speed_abs = torch.abs(_denormalize_speed_mps(reg_pred[..., 4]) - _denormalize_speed_mps(target_speed))
    heading_abs = _circular_abs_diff_deg(_heading_norm_to_deg(reg_pred[..., 5]), _heading_norm_to_deg(target_heading))

    return {
        "count": float(count),
        "alive_correct": float(alive_correct[valid].sum().item()),
        "missile_abs_sum": float(missile_abs[valid].sum().item()),
        "missile_sq_sum": float(missile_sq[valid].sum().item()),
        "position_error_sum": float(position_error[valid].sum().item()),
        "alt_abs_sum": float(alt_abs[valid].sum().item()),
        "speed_abs_sum": float(speed_abs[valid].sum().item()),
        "heading_abs_sum": float(heading_abs[valid].sum().item()),
        "pred_alive_ratio": _mean_or_zero(pred_alive_prob, valid),
        "true_alive_ratio": _mean_or_zero(target_alive, valid),
        "pred_mean_missile": _mean_or_zero(pred_missile, valid),
        "true_mean_missile": _mean_or_zero(true_missile, valid),
    }


def _build_prediction_records_for_batch(
    batch: Dict[str, torch.Tensor],
    outputs: Dict[str, torch.Tensor],
    raw_records: Sequence[Dict[str, Any]],
    reward_norm_stats: Dict[str, Dict[str, float]],
) -> List[Dict[str, Any]]:
    red_alive_logit = outputs["red_alive_logit"].detach().cpu()
    blue_alive_logit = outputs["blue_alive_logit"].detach().cpu()
    red_reg = outputs["red_traj_reg"].detach().cpu()
    blue_reg = outputs["blue_traj_reg"].detach().cpu()

    red_mask = batch["red_mask"].detach().cpu()
    blue_mask = batch["blue_mask"].detach().cpu()
    red_target_mask = batch["red_target_mask"].detach().cpu()
    blue_target_mask = batch["blue_target_mask"].detach().cpu()
    red_target = batch["red_target"].detach().cpu()
    blue_target = batch["blue_target"].detach().cpu()
    terminal_red_win = batch["terminal_red_win"].detach().cpu()
    terminal_red_alive_ratio = batch["terminal_red_alive_ratio"].detach().cpu()
    terminal_blue_alive_ratio = batch["terminal_blue_alive_ratio"].detach().cpu()
    terminal_red_mean_missile = _denormalize_missile(batch["terminal_red_mean_missile"].detach().cpu())
    terminal_blue_mean_missile = _denormalize_missile(batch["terminal_blue_mean_missile"].detach().cpu())
    is_terminal_horizon = batch["is_terminal_horizon"].detach().cpu()
    event_targets = batch["event_flags"].detach().cpu()
    event_count_targets = batch["event_counts"].detach().cpu()
    reward_targets = batch["reward_target"].detach().cpu()

    red_win_prob = torch.sigmoid(outputs["red_win_logit"].detach().cpu())
    pred_terminal_red_alive_ratio = outputs["terminal_red_alive_ratio"].detach().cpu()
    pred_terminal_blue_alive_ratio = outputs["terminal_blue_alive_ratio"].detach().cpu()
    pred_terminal_red_mean_missile = _denormalize_missile(outputs["terminal_red_mean_missile"].detach().cpu())
    pred_terminal_blue_mean_missile = _denormalize_missile(outputs["terminal_blue_mean_missile"].detach().cpu())
    pred_event_probs = torch.sigmoid(outputs["event_flag_logits"].detach().cpu())
    pred_event_counts = torch.clamp(outputs["event_count_pred"].detach().cpu(), min=0.0)
    pred_reward = outputs["reward_pred"].detach().cpu()
    pred_reward_denorm = denormalize_reward_tensor(pred_reward, reward_norm_stats)
    reward_targets_denorm = denormalize_reward_tensor(reward_targets, reward_norm_stats)
    pred_terminal_role_logits = outputs["terminal_critical_role_logits"].detach().cpu()
    pred_terminal_role_ids = torch.argmax(pred_terminal_role_logits, dim=1)
    target_terminal_role_ids = batch["terminal_critical_role_id"].detach().cpu()
    target_terminal_role_masks = batch["terminal_critical_role_mask"].detach().cpu()

    event_flag_index = {key: idx for idx, key in enumerate(EVENT_FLAG_KEYS)}
    event_count_index = {key: idx for idx, key in enumerate(EVENT_COUNT_KEYS)}

    sample_records: List[Dict[str, Any]] = []
    for index, raw_record in enumerate(raw_records):
        red_summary = _team_sample_summary(
            red_alive_logit[index],
            red_reg[index],
            red_target[index],
            red_mask[index],
            red_target_mask[index],
        )
        blue_summary = _team_sample_summary(
            blue_alive_logit[index],
            blue_reg[index],
            blue_target[index],
            blue_mask[index],
            blue_target_mask[index],
        )

        total_count = max(red_summary["count"] + blue_summary["count"], 1.0)
        trajectory_alive_accuracy = (red_summary["alive_correct"] + blue_summary["alive_correct"]) / total_count
        trajectory_missile_mae = (red_summary["missile_abs_sum"] + blue_summary["missile_abs_sum"]) / total_count
        trajectory_missile_mse = (red_summary["missile_sq_sum"] + blue_summary["missile_sq_sum"]) / total_count
        trajectory_position_error_m = (red_summary["position_error_sum"] + blue_summary["position_error_sum"]) / total_count
        trajectory_alt_mae_m = (red_summary["alt_abs_sum"] + blue_summary["alt_abs_sum"]) / total_count
        trajectory_speed_mae_mps = (red_summary["speed_abs_sum"] + blue_summary["speed_abs_sum"]) / total_count
        trajectory_heading_mae_deg = (red_summary["heading_abs_sum"] + blue_summary["heading_abs_sum"]) / total_count

        consistency_alive_ratio_mae = None
        consistency_missile_mae = None
        consistency_total_mae = None
        if float(is_terminal_horizon[index].item()) > 0.5:
            consistency_alive_ratio_mae = 0.5 * (
                abs(red_summary["pred_alive_ratio"] - float(pred_terminal_red_alive_ratio[index].item()))
                + abs(blue_summary["pred_alive_ratio"] - float(pred_terminal_blue_alive_ratio[index].item()))
            )
            consistency_missile_mae = 0.5 * (
                abs(red_summary["pred_mean_missile"] - float(pred_terminal_red_mean_missile[index].item()))
                + abs(blue_summary["pred_mean_missile"] - float(pred_terminal_blue_mean_missile[index].item()))
            )
            consistency_total_mae = 0.5 * (consistency_alive_ratio_mae + consistency_missile_mae)

        event_flag_accuracy = float(
            (((pred_event_probs[index] >= 0.5).float() == event_targets[index]).float().mean()).item()
        )
        event_first_contact_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [event_flag_index["red_first_contact_flag"], event_flag_index["blue_first_contact_flag"]],
        )
        event_first_fire_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [event_flag_index["red_first_fire_flag"], event_flag_index["blue_first_fire_flag"]],
        )
        event_warning_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [event_flag_index["red_warning_flag"], event_flag_index["blue_warning_flag"]],
        )
        event_retarget_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [event_flag_index["red_retarget_flag"], event_flag_index["blue_retarget_flag"]],
        )
        event_first_kill_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [event_flag_index["red_first_kill_flag"], event_flag_index["blue_first_kill_flag"]],
        )
        red_first_kill_prob = float(pred_event_probs[index, event_flag_index["red_first_kill_flag"]].item())
        red_first_kill_target = float(event_targets[index, event_flag_index["red_first_kill_flag"]].item())
        blue_first_kill_prob = float(pred_event_probs[index, event_flag_index["blue_first_kill_flag"]].item())
        blue_first_kill_target = float(event_targets[index, event_flag_index["blue_first_kill_flag"]].item())
        red_first_kill_accuracy = _binary_single_accuracy(red_first_kill_prob, red_first_kill_target)
        blue_first_kill_accuracy = _binary_single_accuracy(blue_first_kill_prob, blue_first_kill_target)
        red_first_kill_positive_recall = _positive_recall_value(red_first_kill_prob, red_first_kill_target)
        blue_first_kill_positive_recall = _positive_recall_value(blue_first_kill_prob, blue_first_kill_target)
        red_first_kill_precision = _precision_value(red_first_kill_prob, red_first_kill_target)
        blue_first_kill_precision = _precision_value(blue_first_kill_prob, blue_first_kill_target)
        red_first_kill_high_conf_hit_rate = _high_conf_hit_value(red_first_kill_prob, red_first_kill_target)
        blue_first_kill_high_conf_hit_rate = _high_conf_hit_value(blue_first_kill_prob, blue_first_kill_target)
        event_objective_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [event_flag_index["red_objective_complete_flag"], event_flag_index["blue_objective_complete_flag"]],
        )
        event_termination_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [event_flag_index["termination_flag"]],
        )
        termination_target = float(event_targets[index, event_flag_index["termination_flag"]].item())
        termination_prob = float(pred_event_probs[index, event_flag_index["termination_flag"]].item())
        termination_high_conf_hit = None
        if termination_target >= 0.5:
            termination_high_conf_hit = 1.0 if termination_prob >= 0.7 else 0.0

        critical_event_accuracy = _binary_accuracy(
            pred_event_probs[index : index + 1],
            event_targets[index : index + 1],
            [
                event_flag_index["red_first_kill_flag"],
                event_flag_index["blue_first_kill_flag"],
                event_flag_index["red_objective_complete_flag"],
                event_flag_index["blue_objective_complete_flag"],
                event_flag_index["termination_flag"],
            ],
        )
        event_fire_count_mae = float(
            torch.abs(
                pred_event_counts[index, [
                    event_count_index["red_fire_count_delta"],
                    event_count_index["blue_fire_count_delta"],
                ]]
                - event_count_targets[index, [
                    event_count_index["red_fire_count_delta"],
                    event_count_index["blue_fire_count_delta"],
                ]]
            ).mean().item()
        )
        event_kill_count_mae = float(
            torch.abs(
                pred_event_counts[index, [
                    event_count_index["red_kill_delta"],
                    event_count_index["blue_kill_delta"],
                ]]
                - event_count_targets[index, [
                    event_count_index["red_kill_delta"],
                    event_count_index["blue_kill_delta"],
                ]]
            ).mean().item()
        )
        event_dodge_count_mae = float(
            torch.abs(
                pred_event_counts[index, [
                    event_count_index["red_dodge_trigger_count"],
                    event_count_index["blue_dodge_trigger_count"],
                ]]
                - event_count_targets[index, [
                    event_count_index["red_dodge_trigger_count"],
                    event_count_index["blue_dodge_trigger_count"],
                ]]
            ).mean().item()
        )
        reward_red_mae = abs(float(pred_reward_denorm[index, 0].item()) - float(reward_targets_denorm[index, 0].item()))
        reward_blue_mae = abs(float(pred_reward_denorm[index, 1].item()) - float(reward_targets_denorm[index, 1].item()))
        reward_total_mae = 0.5 * (reward_red_mae + reward_blue_mae)
        reward_red_delta_scale = abs(float(pred_reward_denorm[index, 0].item()))
        reward_blue_delta_scale = abs(float(pred_reward_denorm[index, 1].item()))
        reward_red_true_delta_scale = abs(float(reward_targets_denorm[index, 0].item()))
        reward_blue_true_delta_scale = abs(float(reward_targets_denorm[index, 1].item()))

        meta = raw_record.get("meta", {})
        sampling_meta = meta.get("sampling_meta", {}) if isinstance(meta.get("sampling_meta", {}), dict) else {}
        blue_first_kill_eligible = bool(meta.get("blue_first_kill_eligible", not bool(meta.get("prior_kill_seen", False))))
        blue_first_kill_terminal_conflict = bool(meta.get("blue_first_kill_terminal_conflict", False))
        terminal_red_outcome_conflict = bool(meta.get("terminal_red_outcome_conflict", False))
        terminal_blue_objective_conflict = bool(
            meta.get("terminal_blue_objective_without_blue_first_kill_conflict", False)
        )
        tactic_combo_key = sampling_meta.get("tactic_combo_key")
        if tactic_combo_key is None:
            red_cond = raw_record.get("red_tactic_condition", {})
            blue_cond = raw_record.get("blue_tactic_condition", {})
            tactic_combo_key = f"{red_cond.get('id', 'red_unknown')}__{blue_cond.get('id', 'blue_unknown')}"

        state_signature = canonical_signature_from_record(raw_record)
        terminal_role_mask = float(target_terminal_role_masks[index].item())
        pred_terminal_role_id = int(pred_terminal_role_ids[index].item())
        true_terminal_role_id = int(target_terminal_role_ids[index].item())
        pred_terminal_role = TERMINAL_CRITICAL_ROLE_LABELS[pred_terminal_role_id]
        true_terminal_role = TERMINAL_CRITICAL_ROLE_LABELS[true_terminal_role_id]
        terminal_role_accuracy = (
            1.0 if (terminal_role_mask > 0.5 and pred_terminal_role_id == true_terminal_role_id) else 0.0
            if terminal_role_mask > 0.5
            else None
        )

        sample_records.append(
            {
                "task_id": meta.get("task_id"),
                "episode_id": meta.get("episode_id"),
                "split_name": meta.get("split_name", "unknown"),
                "split_group_key": meta.get("split_group_key"),
                "scenario_key": sampling_meta.get("scenario_key"),
                "state_signature": state_signature,
                "horizon": raw_record.get("horizon"),
                "is_terminal_horizon": bool(float(is_terminal_horizon[index].item()) > 0.5),
                "t_index": meta.get("t_index"),
                "state_step": meta.get("state_step"),
                "target_step": meta.get("target_step"),
                "terminal_confusion_bucket": meta.get("terminal_confusion_bucket", "none"),
                "tactic_pair_key": sampling_meta.get("tactic_pair_key"),
                "tactic_combo_key": tactic_combo_key,
                "red_tactic_id": sampling_meta.get("red_tactic_id"),
                "blue_tactic_id": sampling_meta.get("blue_tactic_id"),
                "force_size_key": sampling_meta.get("force_size_key"),
                "red_attr_bucket": sampling_meta.get("red_attr_bucket"),
                "blue_attr_bucket": sampling_meta.get("blue_attr_bucket"),
                "trajectory_alive_accuracy": trajectory_alive_accuracy,
                "trajectory_missile_mae": trajectory_missile_mae,
                "trajectory_missile_mse": trajectory_missile_mse,
                "trajectory_position_error_m": trajectory_position_error_m,
                "trajectory_alt_mae_m": trajectory_alt_mae_m,
                "trajectory_speed_mae_mps": trajectory_speed_mae_mps,
                "trajectory_heading_mae_deg": trajectory_heading_mae_deg,
                "terminal_win_accuracy": float(
                    ((red_win_prob[index] >= 0.5).float() == terminal_red_win[index]).item()
                ),
                "terminal_red_alive_ratio_mae": abs(
                    float(pred_terminal_red_alive_ratio[index].item()) - float(terminal_red_alive_ratio[index].item())
                ),
                "terminal_blue_alive_ratio_mae": abs(
                    float(pred_terminal_blue_alive_ratio[index].item()) - float(terminal_blue_alive_ratio[index].item())
                ),
                "terminal_red_mean_missile_mae": abs(
                    float(pred_terminal_red_mean_missile[index].item()) - float(terminal_red_mean_missile[index].item())
                ),
                "terminal_blue_mean_missile_mae": abs(
                    float(pred_terminal_blue_mean_missile[index].item()) - float(terminal_blue_mean_missile[index].item())
                ),
                "consistency_alive_ratio_mae": consistency_alive_ratio_mae,
                "consistency_missile_mae": consistency_missile_mae,
                "consistency_total_mae": consistency_total_mae,
                "event_flag_accuracy": event_flag_accuracy,
                "event_first_contact_accuracy": event_first_contact_accuracy,
                "event_first_fire_accuracy": event_first_fire_accuracy,
                "event_warning_accuracy": event_warning_accuracy,
                "event_retarget_accuracy": event_retarget_accuracy,
                "event_first_kill_accuracy": event_first_kill_accuracy,
                "red_first_kill_accuracy": red_first_kill_accuracy,
                "blue_first_kill_accuracy": blue_first_kill_accuracy,
                "red_first_kill_positive_recall": red_first_kill_positive_recall,
                "blue_first_kill_positive_recall": blue_first_kill_positive_recall,
                "red_first_kill_precision": red_first_kill_precision,
                "blue_first_kill_precision": blue_first_kill_precision,
                "red_first_kill_high_conf_hit_rate": red_first_kill_high_conf_hit_rate,
                "blue_first_kill_high_conf_hit_rate": blue_first_kill_high_conf_hit_rate,
                "blue_first_kill_pred_positive_rate": _pred_positive_value(blue_first_kill_prob),
                "blue_first_kill_false_positive_rate": _false_positive_value(blue_first_kill_prob, blue_first_kill_target),
                "blue_first_kill_terminal_false_positive_rate": (
                    1.0
                    if (
                        str(raw_record.get("horizon")) == "terminal"
                        and blue_first_kill_target < 0.5
                        and blue_first_kill_prob >= 0.5
                    )
                    else (
                        0.0
                        if str(raw_record.get("horizon")) == "terminal" and blue_first_kill_target < 0.5
                        else None
                    )
                ),
                "blue_first_kill_eligible": blue_first_kill_eligible,
                "blue_first_kill_eligible_subset_accuracy": (
                    blue_first_kill_accuracy if blue_first_kill_eligible else None
                ),
                "blue_first_kill_eligible_subset_precision": (
                    blue_first_kill_precision if blue_first_kill_eligible else None
                ),
                "terminal_red_outcome_confusion_rate": (
                    1.0
                    if (
                        terminal_red_outcome_conflict
                        and blue_first_kill_target < 0.5
                        and blue_first_kill_prob >= 0.5
                    )
                    else (0.0 if terminal_red_outcome_conflict else None)
                ),
                "terminal_blue_objective_confusion_rate": (
                    1.0
                    if (
                        terminal_blue_objective_conflict
                        and blue_first_kill_target < 0.5
                        and blue_first_kill_prob >= 0.5
                    )
                    else (0.0 if terminal_blue_objective_conflict else None)
                ),
                "event_objective_accuracy": event_objective_accuracy,
                "event_termination_accuracy": event_termination_accuracy,
                "critical_event_accuracy": critical_event_accuracy,
                "termination_flag_accuracy": event_termination_accuracy,
                "termination_flag_high_conf_hit_rate": termination_high_conf_hit,
                "first_kill_accuracy": event_first_kill_accuracy,
                "objective_complete_accuracy": event_objective_accuracy,
                "event_fire_count_mae": event_fire_count_mae,
                "event_kill_count_mae": event_kill_count_mae,
                "event_dodge_count_mae": event_dodge_count_mae,
                "reward_red_mae": reward_red_mae,
                "reward_blue_mae": reward_blue_mae,
                "reward_total_mae": reward_total_mae,
                "reward_red_delta_scale": reward_red_delta_scale,
                "reward_blue_delta_scale": reward_blue_delta_scale,
                "reward_red_true_delta_scale": reward_red_true_delta_scale,
                "reward_blue_true_delta_scale": reward_blue_true_delta_scale,
                "pred_terminal_critical_role_id": pred_terminal_role_id,
                "true_terminal_critical_role_id": true_terminal_role_id,
                "pred_terminal_critical_role": pred_terminal_role,
                "true_terminal_critical_role": true_terminal_role,
                "terminal_critical_role_accuracy": terminal_role_accuracy,
                "pred_target_red_alive_ratio": red_summary["pred_alive_ratio"],
                "true_target_red_alive_ratio": red_summary["true_alive_ratio"],
                "pred_target_blue_alive_ratio": blue_summary["pred_alive_ratio"],
                "true_target_blue_alive_ratio": blue_summary["true_alive_ratio"],
                "pred_target_red_mean_missile": red_summary["pred_mean_missile"],
                "true_target_red_mean_missile": red_summary["true_mean_missile"],
                "pred_target_blue_mean_missile": blue_summary["pred_mean_missile"],
                "true_target_blue_mean_missile": blue_summary["true_mean_missile"],
                "pred_red_win_prob": float(red_win_prob[index].item()),
                "true_red_win": float(terminal_red_win[index].item()),
                "pred_terminal_red_alive_ratio": float(pred_terminal_red_alive_ratio[index].item()),
                "true_terminal_red_alive_ratio": float(terminal_red_alive_ratio[index].item()),
                "pred_terminal_blue_alive_ratio": float(pred_terminal_blue_alive_ratio[index].item()),
                "true_terminal_blue_alive_ratio": float(terminal_blue_alive_ratio[index].item()),
                "pred_terminal_red_mean_missile": float(pred_terminal_red_mean_missile[index].item()),
                "true_terminal_red_mean_missile": float(terminal_red_mean_missile[index].item()),
                "pred_terminal_blue_mean_missile": float(pred_terminal_blue_mean_missile[index].item()),
                "true_terminal_blue_mean_missile": float(terminal_blue_mean_missile[index].item()),
                "pred_event_red_first_contact_prob": float(
                    pred_event_probs[index, event_flag_index["red_first_contact_flag"]].item()
                ),
                "true_event_red_first_contact": float(
                    event_targets[index, event_flag_index["red_first_contact_flag"]].item()
                ),
                "pred_event_blue_first_contact_prob": float(
                    pred_event_probs[index, event_flag_index["blue_first_contact_flag"]].item()
                ),
                "true_event_blue_first_contact": float(
                    event_targets[index, event_flag_index["blue_first_contact_flag"]].item()
                ),
                "pred_event_red_first_fire_prob": float(
                    pred_event_probs[index, event_flag_index["red_first_fire_flag"]].item()
                ),
                "true_event_red_first_fire": float(
                    event_targets[index, event_flag_index["red_first_fire_flag"]].item()
                ),
                "pred_event_blue_first_fire_prob": float(
                    pred_event_probs[index, event_flag_index["blue_first_fire_flag"]].item()
                ),
                "true_event_blue_first_fire": float(
                    event_targets[index, event_flag_index["blue_first_fire_flag"]].item()
                ),
                "pred_event_red_fire_count": float(
                    pred_event_counts[index, event_count_index["red_fire_count_delta"]].item()
                ),
                "true_event_red_fire_count": float(
                    event_count_targets[index, event_count_index["red_fire_count_delta"]].item()
                ),
                "pred_event_blue_fire_count": float(
                    pred_event_counts[index, event_count_index["blue_fire_count_delta"]].item()
                ),
                "true_event_blue_fire_count": float(
                    event_count_targets[index, event_count_index["blue_fire_count_delta"]].item()
                ),
                "pred_event_red_kill_count": float(
                    pred_event_counts[index, event_count_index["red_kill_delta"]].item()
                ),
                "true_event_red_kill_count": float(
                    event_count_targets[index, event_count_index["red_kill_delta"]].item()
                ),
                "pred_event_blue_kill_count": float(
                    pred_event_counts[index, event_count_index["blue_kill_delta"]].item()
                ),
                "true_event_blue_kill_count": float(
                    event_count_targets[index, event_count_index["blue_kill_delta"]].item()
                ),
                "pred_event_red_retarget_prob": float(
                    pred_event_probs[index, event_flag_index["red_retarget_flag"]].item()
                ),
                "true_event_red_retarget": float(
                    event_targets[index, event_flag_index["red_retarget_flag"]].item()
                ),
                "pred_event_blue_retarget_prob": float(
                    pred_event_probs[index, event_flag_index["blue_retarget_flag"]].item()
                ),
                "true_event_blue_retarget": float(
                    event_targets[index, event_flag_index["blue_retarget_flag"]].item()
                ),
                "pred_event_red_first_kill_prob": red_first_kill_prob,
                "true_event_red_first_kill": red_first_kill_target,
                "pred_event_blue_first_kill_prob": blue_first_kill_prob,
                "true_event_blue_first_kill": blue_first_kill_target,
                "pred_event_red_objective_complete_prob": float(
                    pred_event_probs[index, event_flag_index["red_objective_complete_flag"]].item()
                ),
                "true_event_red_objective_complete": float(
                    event_targets[index, event_flag_index["red_objective_complete_flag"]].item()
                ),
                "pred_event_blue_objective_complete_prob": float(
                    pred_event_probs[index, event_flag_index["blue_objective_complete_flag"]].item()
                ),
                "true_event_blue_objective_complete": float(
                    event_targets[index, event_flag_index["blue_objective_complete_flag"]].item()
                ),
                "pred_reward_red": float(pred_reward[index, 0].item()),
                "true_reward_red": float(reward_targets[index, 0].item()),
                "pred_reward_blue": float(pred_reward[index, 1].item()),
                "true_reward_blue": float(reward_targets[index, 1].item()),
                "pred_reward_red_denorm": float(pred_reward_denorm[index, 0].item()),
                "true_reward_red_denorm": float(reward_targets_denorm[index, 0].item()),
                "pred_reward_blue_denorm": float(pred_reward_denorm[index, 1].item()),
                "true_reward_blue_denorm": float(reward_targets_denorm[index, 1].item()),
                "pred_event_termination_prob": termination_prob,
                "true_event_termination": termination_target,
            }
        )
    return sample_records


def summarize_prediction_records(
    sample_records: Sequence[Dict[str, Any]],
    loss_sums: Dict[str, float],
    total_samples: int,
) -> Dict[str, Any]:
    summary: Dict[str, Any] = {
        "num_samples": total_samples,
        "num_episodes": len({record.get("episode_id") for record in sample_records if record.get("episode_id")}),
        "num_split_groups": len({record.get("split_group_key") for record in sample_records if record.get("split_group_key")}),
        "num_terminal_horizon_samples": sum(1 for record in sample_records if record.get("is_terminal_horizon")),
        "horizon_counts": dict(sorted(Counter(str(record.get("horizon")) for record in sample_records).items())),
    }

    denom = max(total_samples, 1)
    summary["loss_total"] = loss_sums["total"] / denom
    summary["loss_trajectory"] = loss_sums["trajectory"] / denom
    summary["loss_terminal"] = loss_sums["terminal"] / denom
    summary["loss_consistency"] = loss_sums["consistency"] / denom
    summary["loss_event"] = loss_sums["event"] / denom
    summary["loss_reward"] = loss_sums["reward"] / denom

    for metric_key in SUMMARY_METRIC_KEYS:
        values = [float(record[metric_key]) for record in sample_records if record.get(metric_key) is not None]
        summary[metric_key] = (sum(values) / len(values)) if values else None

    return summary


def summarize_prediction_records_without_losses(sample_records: Sequence[Dict[str, Any]]) -> Dict[str, Any]:
    summary = summarize_prediction_records(
        sample_records=sample_records,
        loss_sums={
            "total": 0.0,
            "trajectory": 0.0,
            "terminal": 0.0,
            "consistency": 0.0,
            "event": 0.0,
            "reward": 0.0,
        },
        total_samples=len(sample_records),
    )
    for key in ("loss_total", "loss_trajectory", "loss_terminal", "loss_consistency", "loss_event", "loss_reward"):
        summary.pop(key, None)
    return summary


def aggregate_bucket_metrics(
    sample_records: Sequence[Dict[str, Any]],
    bucket_keys: Sequence[str] = DEFAULT_BUCKET_KEYS,
) -> Dict[str, Dict[str, Dict[str, Any]]]:
    bucket_summary: Dict[str, Dict[str, Dict[str, Any]]] = {}
    for bucket_key in bucket_keys:
        grouped: DefaultDict[str, List[Dict[str, Any]]] = defaultdict(list)
        for record in sample_records:
            grouped[_extract_bucket_value(record, bucket_key)].append(record)

        ordered_items = sorted(grouped.items(), key=lambda item: (-len(item[1]), item[0]))
        bucket_summary[bucket_key] = {}
        for bucket_value, bucket_records in ordered_items:
            bucket_summary[bucket_key][bucket_value] = summarize_prediction_records_without_losses(bucket_records)
    return bucket_summary


def collect_prediction_records(
    model: WorldModelNet,
    records: List[Dict],
    batch_size: int,
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
    loss_config: Dict[str, Any] = None,
) -> Tuple[Dict[str, Any], List[Dict[str, Any]]]:
    if not records:
        empty = summarize_prediction_records(
            sample_records=[],
            loss_sums={
                "total": 0.0,
                "trajectory": 0.0,
                "terminal": 0.0,
                "consistency": 0.0,
                "event": 0.0,
                "reward": 0.0,
            },
            total_samples=0,
        )
        return empty, []

    loader = make_data_loader(records, batch_size=batch_size, shuffle=False, reward_norm_stats=reward_norm_stats)
    model.eval()

    total_samples = 0
    loss_sums = {
        "total": 0.0,
        "trajectory": 0.0,
        "terminal": 0.0,
        "consistency": 0.0,
        "event": 0.0,
        "reward": 0.0,
    }
    prediction_records: List[Dict[str, Any]] = []
    cursor = 0

    with torch.no_grad():
        for batch in loader:
            batch = move_to_device(batch, device)
            outputs, losses = compute_losses(model, batch, loss_config=loss_config)

            batch_size_actual = int(batch["terminal_red_win"].shape[0])
            raw_batch_records = records[cursor : cursor + batch_size_actual]
            prediction_records.extend(
                _build_prediction_records_for_batch(
                    batch,
                    outputs,
                    raw_batch_records,
                    reward_norm_stats=reward_norm_stats,
                )
            )
            cursor += batch_size_actual

            total_samples += batch_size_actual
            loss_sums["total"] += float(losses["total"].item()) * batch_size_actual
            loss_sums["trajectory"] += float(losses["trajectory"].item()) * batch_size_actual
            loss_sums["terminal"] += float(losses["terminal"].item()) * batch_size_actual
            loss_sums["consistency"] += float(losses["consistency"].item()) * batch_size_actual
            loss_sums["event"] += float(losses["event"].item()) * batch_size_actual
            loss_sums["reward"] += float(losses["reward"].item()) * batch_size_actual

    return summarize_prediction_records(prediction_records, loss_sums, total_samples), prediction_records


def evaluate_records(
    model: WorldModelNet,
    records: List[Dict],
    batch_size: int,
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
    bucket_keys: Sequence[str] = DEFAULT_BUCKET_KEYS,
    loss_config: Dict[str, Any] = None,
) -> Dict[str, Any]:
    aggregate, prediction_records = collect_prediction_records(
        model,
        records,
        batch_size,
        device,
        reward_norm_stats=reward_norm_stats,
        loss_config=loss_config,
    )
    return {
        "aggregate": aggregate,
        "bucket_metrics": aggregate_bucket_metrics(prediction_records, bucket_keys),
    }


def evaluate(
    model: WorldModelNet,
    loader: DataLoader,
    device: torch.device,
    loss_config: Dict[str, Any] = None,
) -> Dict[str, float]:
    model.eval()
    total_samples = 0
    total_metrics = {
        "loss_total": 0.0,
        "loss_trajectory": 0.0,
        "loss_terminal": 0.0,
        "loss_consistency": 0.0,
        "loss_event": 0.0,
        "loss_reward": 0.0,
        "loss_terminal_role": 0.0,
    }
    win_correct = 0

    with torch.no_grad():
        for batch in loader:
            batch = move_to_device(batch, device)
            outputs, losses = compute_losses(model, batch, loss_config=loss_config)
            batch_size = int(batch["terminal_red_win"].shape[0])
            total_samples += batch_size
            total_metrics["loss_total"] += float(losses["total"].item()) * batch_size
            total_metrics["loss_trajectory"] += float(losses["trajectory"].item()) * batch_size
            total_metrics["loss_terminal"] += float(losses["terminal"].item()) * batch_size
            total_metrics["loss_consistency"] += float(losses["consistency"].item()) * batch_size
            total_metrics["loss_event"] += float(losses["event"].item()) * batch_size
            total_metrics["loss_reward"] += float(losses["reward"].item()) * batch_size
            total_metrics["loss_terminal_role"] += float(losses["terminal_role"].item()) * batch_size
            win_correct += int(((outputs["red_win_logit"] >= 0.0).float() == batch["terminal_red_win"]).sum().item())

    denom = max(total_samples, 1)
    total_metrics["win_accuracy"] = win_correct / denom
    total_metrics["num_samples"] = total_samples
    for key in (
        "loss_total",
        "loss_trajectory",
        "loss_terminal",
        "loss_consistency",
        "loss_event",
        "loss_reward",
        "loss_terminal_role",
    ):
        total_metrics[key] /= denom
    return total_metrics


def run_counterfactual_finetune(
    model: WorldModelNet,
    pair_entries: Sequence[Dict[str, Any]],
    val_loader: DataLoader,
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
    epochs: int,
    batch_size: int,
    lr: float,
    loss_weight: float,
    seed: int,
) -> List[Dict[str, Any]]:
    if not pair_entries or epochs <= 0:
        return []

    optimizer = torch.optim.Adam(model.parameters(), lr=lr)
    rng = random.Random(seed)
    history: List[Dict[str, Any]] = []

    for epoch in range(1, epochs + 1):
        shuffled_pairs = list(pair_entries)
        rng.shuffle(shuffled_pairs)

        model.train()
        total_epoch_loss = 0.0
        total_sample_loss = 0.0
        total_delta_loss = 0.0
        total_pairs = 0
        metric_loss_sums = {metric_name: 0.0 for metric_name in COUNTERFACTUAL_METRIC_SPECS}
        pred_delta_sums = {metric_name: 0.0 for metric_name in COUNTERFACTUAL_METRIC_SPECS}
        true_delta_sums = {metric_name: 0.0 for metric_name in COUNTERFACTUAL_METRIC_SPECS}

        for start in range(0, len(shuffled_pairs), batch_size):
            batch_pairs = shuffled_pairs[start : start + batch_size]
            pair_result = compute_counterfactual_pair_loss(
                model,
                batch_pairs,
                device,
                reward_norm_stats=reward_norm_stats,
                loss_config=STAGE2_LOSS_CONFIG,
            )
            total_loss = pair_result["sample_loss"] + loss_weight * pair_result["delta_loss"]

            optimizer.zero_grad()
            total_loss.backward()
            optimizer.step()

            pair_count = len(batch_pairs)
            total_epoch_loss += float(total_loss.item()) * pair_count
            total_sample_loss += float(pair_result["sample_loss"].item()) * pair_count
            total_delta_loss += float(pair_result["delta_loss"].item()) * pair_count
            total_pairs += pair_count
            for metric_name in COUNTERFACTUAL_METRIC_SPECS:
                metric_loss_sums[metric_name] += pair_result["metric_losses"][metric_name] * pair_count
                pred_delta_sums[metric_name] += pair_result["mean_pred_abs_delta"][metric_name] * pair_count
                true_delta_sums[metric_name] += pair_result["mean_true_abs_delta"][metric_name] * pair_count

        val_metrics = evaluate(model, val_loader, device, loss_config=DEFAULT_LOSS_CONFIG) if len(val_loader.dataset) > 0 else {
            "loss_total": 0.0,
            "loss_trajectory": 0.0,
            "loss_terminal": 0.0,
            "loss_consistency": 0.0,
            "loss_event": 0.0,
            "loss_reward": 0.0,
            "win_accuracy": 0.0,
            "num_samples": 0,
        }

        denom = max(total_pairs, 1)
        entry = {
            "phase": "counterfactual_finetune",
            "epoch": epoch,
            "pair_loss_total": total_epoch_loss / denom,
            "pair_loss_sample": total_sample_loss / denom,
            "pair_loss_delta": total_delta_loss / denom,
            "num_pairs": total_pairs,
            "val_total": val_metrics["loss_total"],
            "val_win_acc": val_metrics["win_accuracy"],
        }
        for metric_name in COUNTERFACTUAL_METRIC_SPECS:
            entry[f"{metric_name}_delta_loss"] = metric_loss_sums[metric_name] / denom
            entry[f"{metric_name}_mean_pred_abs_delta"] = pred_delta_sums[metric_name] / denom
            entry[f"{metric_name}_mean_true_abs_delta"] = true_delta_sums[metric_name] / denom
        history.append(entry)

        print(
            f"CF Epoch {epoch:03d} | pair_total={entry['pair_loss_total']:.4f} | "
            f"pair_delta={entry['pair_loss_delta']:.4f} | "
            f"pred_win_delta={entry['terminal_red_win_prob_mean_pred_abs_delta']:.4f} | "
            f"pred_fire_delta={entry['event_red_fire_count_mean_pred_abs_delta']:.4f} | "
            f"val_total={val_metrics['loss_total']:.4f}"
        )

    return history


def run_strict_split_evaluation(
    model: WorldModelNet,
    data_dir: Path,
    batch_size: int,
    device: torch.device,
    reward_norm_stats: Dict[str, Dict[str, float]],
    split_names: Sequence[str] = ("val", "test", "ood"),
) -> Dict[str, Any]:
    results: Dict[str, Any] = {}
    loaded_records: Dict[str, List[Dict]] = {}

    for split_name in split_names:
        records = load_split_records(data_dir, split_name)
        loaded_records[split_name] = records
        if records:
            results[split_name] = evaluate_records(
                model,
                records,
                batch_size,
                device,
                reward_norm_stats=reward_norm_stats,
                loss_config=DEFAULT_LOSS_CONFIG,
            )

    seen_records = loaded_records.get("val", []) + loaded_records.get("test", [])
    if seen_records:
        results["seen_eval"] = evaluate_records(
            model,
            seen_records,
            batch_size,
            device,
            reward_norm_stats=reward_norm_stats,
            loss_config=DEFAULT_LOSS_CONFIG,
        )

    return results


def main() -> None:
    parser = argparse.ArgumentParser(description="Train a minimal tactical world model.")
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--epochs", type=int, default=10)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--init-model-path", type=Path, default=None)
    parser.add_argument("--counterfactual-data-dir", type=Path, default=None)
    parser.add_argument(
        "--counterfactual-train-splits",
        type=str,
        default="train",
        help="Comma-separated processed splits used to mine same-initial counterfactual training pairs.",
    )
    parser.add_argument(
        "--counterfactual-horizons",
        type=str,
        default="1,5,10,20,terminal",
        help="Comma-separated horizons used for counterfactual pair mining.",
    )
    parser.add_argument("--counterfactual-state-step", type=int, default=0)
    parser.add_argument("--counterfactual-min-signal", type=float, default=0.10)
    parser.add_argument("--counterfactual-epochs", type=int, default=0)
    parser.add_argument("--counterfactual-batch-size", type=int, default=4)
    parser.add_argument("--counterfactual-lr", type=float, default=2e-4)
    parser.add_argument("--counterfactual-loss-weight", type=float, default=4.0)
    parser.add_argument("--stage2-epochs", type=int, default=2)
    parser.add_argument("--stage2-lr", type=float, default=3e-4)
    parser.add_argument("--stage2-firstkill-focus", action="store_true")
    parser.add_argument("--stage2-firstkill-positive-repeat", type=int, default=4)
    parser.add_argument("--stage2-firstkill-hard-negative-repeat", type=int, default=2)
    parser.add_argument("--stage2-red-firstkill-positive-repeat", type=int, default=None)
    parser.add_argument("--stage2-blue-firstkill-positive-repeat", type=int, default=None)
    parser.add_argument("--stage2-red-firstkill-hard-negative-repeat", type=int, default=None)
    parser.add_argument("--stage2-blue-firstkill-hard-negative-repeat", type=int, default=None)
    parser.add_argument("--stage2-blue-terminal-conflict-repeat", type=int, default=1)
    parser.add_argument(
        "--target-contract",
        type=str,
        default="baseline",
        choices=["baseline", "phaseA_v1", "phaseC_v1"],
        help="Target-consumption contract used by the training dataset.",
    )
    parser.add_argument(
        "--loss-config-path",
        type=Path,
        default=None,
        help="Optional JSON file to override DEFAULT_LOSS_CONFIG fields.",
    )
    parser.add_argument(
        "--stage2-loss-config-path",
        type=Path,
        default=None,
        help="Optional JSON file to override STAGE2_LOSS_CONFIG fields.",
    )
    args = parser.parse_args()

    set_seed(args.seed)
    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    train_loss_config = (
        _build_loss_config(read_json(args.loss_config_path))
        if args.loss_config_path
        else _build_loss_config(DEFAULT_LOSS_CONFIG)
    )
    stage2_loss_config = (
        _build_loss_config(read_json(args.stage2_loss_config_path))
        if args.stage2_loss_config_path
        else _build_loss_config(STAGE2_LOSS_CONFIG)
    )

    train_records = load_split_records(args.data_dir, "train")
    val_records = load_split_records(args.data_dir, "val")
    test_records = load_split_records(args.data_dir, "test")
    ood_records = load_split_records(args.data_dir, "ood")

    reward_norm_stats = compute_reward_norm_stats(train_records)

    train_ds = WorldModelDataset(
        train_records,
        reward_norm_stats=reward_norm_stats,
        target_contract=args.target_contract,
    )
    val_ds = WorldModelDataset(val_records, reward_norm_stats=reward_norm_stats)
    if len(train_ds) == 0:
        raise RuntimeError("No training samples found for world-model training.")

    train_loader = DataLoader(train_ds, batch_size=args.batch_size, shuffle=True, collate_fn=collate_world_model)
    val_loader = DataLoader(val_ds, batch_size=args.batch_size, shuffle=False, collate_fn=collate_world_model)

    model = WorldModelNet().to(device)
    if args.init_model_path is not None:
        model.load_state_dict(torch.load(args.init_model_path, map_location=device), strict=False)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)

    history = []
    counterfactual_history: List[Dict[str, Any]] = []
    counterfactual_pair_summary: Dict[str, Any] = {}
    stage2_sampling_summary: Dict[str, Any] = {
        "mode": "default",
        "base_count": len(train_records),
        "expanded_count": len(train_records),
    }
    best_val = float("inf")
    args.out_dir.mkdir(parents=True, exist_ok=True)

    for epoch in range(1, args.epochs + 1):
        model.train()
        train_loss_sum = 0.0
        train_count = 0

        for batch in train_loader:
            batch = move_to_device(batch, device)
            _, losses = compute_losses(model, batch, loss_config=train_loss_config)

            optimizer.zero_grad()
            losses["total"].backward()
            optimizer.step()

            batch_size = int(batch["terminal_red_win"].shape[0])
            train_loss_sum += float(losses["total"].item()) * batch_size
            train_count += batch_size

        val_metrics = evaluate(model, val_loader, device, loss_config=DEFAULT_LOSS_CONFIG) if len(val_ds) > 0 else {
            "loss_total": 0.0,
            "loss_trajectory": 0.0,
            "loss_terminal": 0.0,
            "loss_consistency": 0.0,
            "loss_event": 0.0,
            "loss_reward": 0.0,
            "win_accuracy": 0.0,
            "num_samples": 0,
        }
        avg_train_loss = train_loss_sum / max(train_count, 1)
        history.append({"epoch": epoch, "train_loss": avg_train_loss, **val_metrics})
        print(
            f"Epoch {epoch:03d} | train_loss={avg_train_loss:.4f} | "
            f"val_total={val_metrics['loss_total']:.4f} | val_win_acc={val_metrics['win_accuracy']:.4f}"
        )

        if val_metrics["loss_total"] <= best_val:
            best_val = val_metrics["loss_total"]
            torch.save(model.state_dict(), args.out_dir / "model.pt")

    model.load_state_dict(torch.load(args.out_dir / "model.pt", map_location=device), strict=False)

    if args.stage2_epochs > 0:
        stage2_records = train_records
        if args.stage2_firstkill_focus:
            stage2_records, stage2_sampling_summary = build_first_kill_stage2_records(
                train_records,
                positive_repeat=args.stage2_firstkill_positive_repeat,
                hard_negative_repeat=args.stage2_firstkill_hard_negative_repeat,
                red_positive_repeat=args.stage2_red_firstkill_positive_repeat,
                blue_positive_repeat=args.stage2_blue_firstkill_positive_repeat,
                red_hard_negative_repeat=args.stage2_red_firstkill_hard_negative_repeat,
                blue_hard_negative_repeat=args.stage2_blue_firstkill_hard_negative_repeat,
                blue_terminal_conflict_repeat=args.stage2_blue_terminal_conflict_repeat,
            )
            stage2_sampling_summary["mode"] = "first_kill_focus"
        stage2_ds = WorldModelDataset(
            stage2_records,
            reward_norm_stats=reward_norm_stats,
            target_contract=args.target_contract,
        )
        stage2_loader = DataLoader(
            stage2_ds,
            batch_size=args.batch_size,
            shuffle=True,
            collate_fn=collate_world_model,
        )
        stage2_optimizer = torch.optim.Adam(model.parameters(), lr=args.stage2_lr)
        for epoch in range(1, args.stage2_epochs + 1):
            model.train()
            stage2_loss_sum = 0.0
            stage2_count = 0
            for batch in stage2_loader:
                batch = move_to_device(batch, device)
                _, losses = compute_losses(model, batch, loss_config=stage2_loss_config)
                stage2_optimizer.zero_grad()
                losses["total"].backward()
                stage2_optimizer.step()
                bs = int(batch["terminal_red_win"].shape[0])
                stage2_loss_sum += float(losses["total"].item()) * bs
                stage2_count += bs

            val_metrics = evaluate(model, val_loader, device, loss_config=DEFAULT_LOSS_CONFIG) if len(val_ds) > 0 else {
                "loss_total": 0.0,
                "loss_trajectory": 0.0,
                "loss_terminal": 0.0,
                "loss_consistency": 0.0,
                "loss_event": 0.0,
                "loss_reward": 0.0,
                "win_accuracy": 0.0,
                "num_samples": 0,
            }
            avg_stage2_loss = stage2_loss_sum / max(stage2_count, 1)
            history.append(
                {
                    "phase": "stage2_event_reward",
                    "epoch": epoch,
                    "train_loss": avg_stage2_loss,
                    "stage2_sampling_mode": stage2_sampling_summary.get("mode", "default"),
                    **val_metrics,
                }
            )
            print(
                f"Stage2 Epoch {epoch:03d} | train_loss={avg_stage2_loss:.4f} | "
                f"val_total={val_metrics['loss_total']:.4f} | val_event={val_metrics['loss_event']:.4f} | "
                f"val_reward={val_metrics['loss_reward']:.4f}"
            )

        torch.save(model.state_dict(), args.out_dir / "model.pt")
    if args.counterfactual_data_dir is not None and args.counterfactual_epochs > 0:
        counterfactual_split_names = [
            token.strip() for token in args.counterfactual_train_splits.split(",") if token.strip()
        ]
        counterfactual_records: List[Dict[str, Any]] = []
        for split_name in counterfactual_split_names:
            counterfactual_records.extend(load_split_records(args.counterfactual_data_dir, split_name))
        counterfactual_pairs = build_counterfactual_pairs(
            counterfactual_records,
            horizons=[token.strip() for token in args.counterfactual_horizons.split(",") if token.strip()],
            state_step=args.counterfactual_state_step,
            min_signal=args.counterfactual_min_signal,
            include_reverse=True,
        )
        counterfactual_pair_summary = {
            "data_dir": str(args.counterfactual_data_dir),
            "train_splits": counterfactual_split_names,
            "num_records": len(counterfactual_records),
            "num_pairs": len(counterfactual_pairs),
            "state_step": args.counterfactual_state_step,
            "horizons": [token.strip() for token in args.counterfactual_horizons.split(",") if token.strip()],
            "mean_pair_weight": (
                sum(float(entry["pair_weight"]) for entry in counterfactual_pairs) / len(counterfactual_pairs)
                if counterfactual_pairs
                else 0.0
            ),
        }
        print(
            "Counterfactual pairs | "
            f"records={counterfactual_pair_summary['num_records']} | "
            f"pairs={counterfactual_pair_summary['num_pairs']} | "
            f"mean_weight={counterfactual_pair_summary['mean_pair_weight']:.4f}"
        )
        if counterfactual_pairs:
            counterfactual_history = run_counterfactual_finetune(
                model,
                counterfactual_pairs,
                val_loader,
                device,
                reward_norm_stats=reward_norm_stats,
                epochs=args.counterfactual_epochs,
                batch_size=args.counterfactual_batch_size,
                lr=args.counterfactual_lr,
                loss_weight=args.counterfactual_loss_weight,
                seed=args.seed + 101,
                loss_config=stage2_loss_config,
            )
            torch.save(model.state_dict(), args.out_dir / "model.pt")

    strict_eval = run_strict_split_evaluation(
        model,
        args.data_dir,
        args.batch_size,
        device,
        reward_norm_stats=reward_norm_stats,
    )

    processed_summary_path = args.data_dir / "processed" / "summary.json"
    processed_summary = read_json(processed_summary_path) if processed_summary_path.exists() else {}
    write_json(args.out_dir / "train_history.json", {"history": history, "counterfactual_history": counterfactual_history})
    write_json(args.out_dir / "strict_eval.json", strict_eval)
    write_json(
        args.out_dir / "meta.json",
        {
            "seed": args.seed,
            "epochs": args.epochs,
            "batch_size": args.batch_size,
            "lr": args.lr,
            "device": args.device,
            "best_val_loss": best_val,
            "train_samples": len(train_records),
            "val_samples": len(val_records),
            "test_samples": len(test_records),
            "ood_samples": len(ood_records),
            "counterfactual_pair_summary": counterfactual_pair_summary,
            "counterfactual_epochs": args.counterfactual_epochs,
            "counterfactual_lr": args.counterfactual_lr,
            "counterfactual_loss_weight": args.counterfactual_loss_weight,
            "stage2_epochs": args.stage2_epochs,
            "stage2_lr": args.stage2_lr,
            "stage2_firstkill_focus": bool(args.stage2_firstkill_focus),
            "stage2_firstkill_positive_repeat": int(args.stage2_firstkill_positive_repeat),
            "stage2_firstkill_hard_negative_repeat": int(args.stage2_firstkill_hard_negative_repeat),
            "stage2_red_firstkill_positive_repeat": args.stage2_red_firstkill_positive_repeat,
            "stage2_blue_firstkill_positive_repeat": args.stage2_blue_firstkill_positive_repeat,
            "stage2_red_firstkill_hard_negative_repeat": args.stage2_red_firstkill_hard_negative_repeat,
            "stage2_blue_firstkill_hard_negative_repeat": args.stage2_blue_firstkill_hard_negative_repeat,
            "stage2_blue_terminal_conflict_repeat": args.stage2_blue_terminal_conflict_repeat,
            "loss_config_path": str(args.loss_config_path) if args.loss_config_path else None,
            "stage2_loss_config_path": str(args.stage2_loss_config_path) if args.stage2_loss_config_path else None,
            "effective_loss_config": train_loss_config,
            "effective_stage2_loss_config": stage2_loss_config,
            "stage2_sampling_summary": stage2_sampling_summary,
            "target_contract": args.target_contract,
            "reward_norm_stats": reward_norm_stats,
            "processed_summary": processed_summary,
        },
    )

    seen_eval = strict_eval.get("seen_eval", {}).get("aggregate", {})
    ood_eval = strict_eval.get("ood", {}).get("aggregate", {})
    print(f"Training finished. Best val total loss: {best_val:.4f}")
    if seen_eval:
        print(
            "Seen eval | "
            f"alive_acc={seen_eval.get('trajectory_alive_accuracy', 0.0):.4f} | "
            f"pos_err_m={seen_eval.get('trajectory_position_error_m', 0.0):.1f} | "
            f"event_flag_acc={seen_eval.get('event_flag_accuracy', 0.0):.4f} | "
            f"reward_mae={seen_eval.get('reward_total_mae', 0.0):.4f} | "
            f"win_acc={seen_eval.get('terminal_win_accuracy', 0.0):.4f}"
        )
    if ood_eval:
        print(
            "OOD eval  | "
            f"alive_acc={ood_eval.get('trajectory_alive_accuracy', 0.0):.4f} | "
            f"pos_err_m={ood_eval.get('trajectory_position_error_m', 0.0):.1f} | "
            f"event_flag_acc={ood_eval.get('event_flag_accuracy', 0.0):.4f} | "
            f"reward_mae={ood_eval.get('reward_total_mae', 0.0):.4f} | "
            f"win_acc={ood_eval.get('terminal_win_accuracy', 0.0):.4f}"
        )


if __name__ == "__main__":
    main()
