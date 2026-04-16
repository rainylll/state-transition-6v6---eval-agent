import re
from typing import Any, Dict, List, Optional, Tuple

import torch
from torch.utils.data import Dataset

HORIZON_TO_ID = {1: 0, 5: 1, 10: 2, 20: 3, "terminal": 4}
SOURCE_TO_ID = {
    "shared_tactic_id": 0,
    "legacy_tactic_id": 1,
    "task_config": 2,
    "generator": 3,
    "task_generator": 4,
}
FAMILY_TO_ID = {"legacy_shared_tactic": 0, "rule": 1, "rl": 2}
TERMINATION_REASON_TO_ID = {
    "none": 0,
    "red_eliminated": 1,
    "blue_eliminated": 2,
    "objective_complete": 3,
    "safety_limit": 4,
}
NON_DECISIVE_TERMINATION_REASONS = {
    "none",
    "safety_limit",
    "timeout",
    "time_limit",
}
EVENT_FLAG_KEYS = (
    "red_first_contact_flag",
    "blue_first_contact_flag",
    "red_contact_flag",
    "blue_contact_flag",
    "red_first_fire_flag",
    "blue_first_fire_flag",
    "red_warning_flag",
    "blue_warning_flag",
    "red_retarget_flag",
    "blue_retarget_flag",
    "red_first_kill_flag",
    "blue_first_kill_flag",
    "red_objective_complete_flag",
    "blue_objective_complete_flag",
    "termination_flag",
)
EVENT_COUNT_KEYS = (
    "red_fire_count_delta",
    "blue_fire_count_delta",
    "red_kill_delta",
    "blue_kill_delta",
    "red_dodge_trigger_count",
    "blue_dodge_trigger_count",
)
REWARD_KEYS = ("red", "blue")
UNIT_FEATURE_KEYS = (
    "alive",
    "missile_count",
    "lon",
    "lat",
    "alt_m",
    "speed_mps",
    "heading_deg",
)
UNIT_FEATURE_DIM = len(UNIT_FEATURE_KEYS)
TACTIC_FEATURE_DIM = 3
EVENT_FLAG_DIM = len(EVENT_FLAG_KEYS)
EVENT_COUNT_DIM = len(EVENT_COUNT_KEYS)
REWARD_DIM = len(REWARD_KEYS)
_NUMERIC_ID_RE = re.compile(r"-?\d+")


DEFAULT_REWARD_CLIP_LOW = -10.0
DEFAULT_REWARD_CLIP_HIGH = 10.0
DEFAULT_REWARD_MEAN = 0.0
DEFAULT_REWARD_STD = 1.0


def _normalize_heading_deg(value: float) -> float:
    wrapped = float(value) % 360.0
    return wrapped / 180.0 - 1.0


def _parse_numeric_id(value) -> float:
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        try:
            return float(value)
        except ValueError:
            matches = _NUMERIC_ID_RE.findall(value)
            if matches:
                return float(matches[-1])
    return 0.0


def _normalize_unit(unit: Dict) -> Tuple[int, List[float]]:
    type_id = int(round(float(unit.get("type_id", 0.0))))
    features = [
        float(unit.get("alive", 0.0)),
        float(unit.get("missile_count", 0.0)) / 8.0,
        float(unit.get("lon", 0.0)) / 180.0,
        float(unit.get("lat", 0.0)) / 90.0,
        float(unit.get("alt_m", 0.0)) / 20000.0,
        float(unit.get("speed_mps", 0.0)) / 1000.0,
        _normalize_heading_deg(float(unit.get("heading_deg", 0.0))),
    ]
    return type_id, features


def _ordered_unit_ids(*unit_groups: List[Dict]) -> List[str]:
    ordered: List[str] = []
    seen = set()
    for units in unit_groups:
        for unit in units:
            unit_id = str(unit.get("unit_id", ""))
            if not unit_id or unit_id in seen:
                continue
            seen.add(unit_id)
            ordered.append(unit_id)
    return ordered


def _lookup_units(units: List[Dict]) -> Dict[str, Dict]:
    lookup: Dict[str, Dict] = {}
    for unit in units:
        unit_id = str(unit.get("unit_id", ""))
        if unit_id and unit_id not in lookup:
            lookup[unit_id] = unit
    return lookup


def _encode_aligned_side(
    state_units: List[Dict],
    target_units: List[Dict],
    terminal_units: List[Dict],
) -> Dict[str, torch.Tensor]:
    unit_ids = _ordered_unit_ids(state_units, target_units, terminal_units)
    count = len(unit_ids)

    if count == 0:
        zero_features = torch.zeros((0, UNIT_FEATURE_DIM), dtype=torch.float32)
        zero_types = torch.zeros((0,), dtype=torch.long)
        zero_mask = torch.zeros((0,), dtype=torch.bool)
        return {
            "unit_ids": [],
            "state_features": zero_features,
            "target_features": zero_features.clone(),
            "terminal_features": zero_features.clone(),
            "type_ids": zero_types,
            "state_present": zero_mask,
            "target_present": zero_mask.clone(),
            "terminal_present": zero_mask.clone(),
        }

    state_lookup = _lookup_units(state_units)
    target_lookup = _lookup_units(target_units)
    terminal_lookup = _lookup_units(terminal_units)

    state_features = torch.zeros((count, UNIT_FEATURE_DIM), dtype=torch.float32)
    target_features = torch.zeros((count, UNIT_FEATURE_DIM), dtype=torch.float32)
    terminal_features = torch.zeros((count, UNIT_FEATURE_DIM), dtype=torch.float32)
    type_ids = torch.zeros((count,), dtype=torch.long)
    state_present = torch.zeros((count,), dtype=torch.bool)
    target_present = torch.zeros((count,), dtype=torch.bool)
    terminal_present = torch.zeros((count,), dtype=torch.bool)

    for index, unit_id in enumerate(unit_ids):
        reference_unit = state_lookup.get(unit_id) or target_lookup.get(unit_id) or terminal_lookup.get(unit_id) or {}
        type_ids[index] = int(round(float(reference_unit.get("type_id", 0.0))))

        if unit_id in state_lookup:
            _, features = _normalize_unit(state_lookup[unit_id])
            state_features[index] = torch.tensor(features, dtype=torch.float32)
            state_present[index] = True
        if unit_id in target_lookup:
            _, features = _normalize_unit(target_lookup[unit_id])
            target_features[index] = torch.tensor(features, dtype=torch.float32)
            target_present[index] = True
        if unit_id in terminal_lookup:
            _, features = _normalize_unit(terminal_lookup[unit_id])
            terminal_features[index] = torch.tensor(features, dtype=torch.float32)
            terminal_present[index] = True

    return {
        "unit_ids": unit_ids,
        "state_features": state_features,
        "target_features": target_features,
        "terminal_features": terminal_features,
        "type_ids": type_ids,
        "state_present": state_present,
        "target_present": target_present,
        "terminal_present": terminal_present,
    }


def _encode_tactic_condition(condition: Dict) -> torch.Tensor:
    source_id = SOURCE_TO_ID.get(str(condition.get("source", "")), len(SOURCE_TO_ID))
    family_id = FAMILY_TO_ID.get(str(condition.get("family", "")), len(FAMILY_TO_ID))
    params = condition.get("params", {})
    if not isinstance(params, dict):
        params = {}
    tactic_id = _parse_numeric_id(params.get("tactic_id"))
    if tactic_id == 0.0:
        tactic_id = _parse_numeric_id(condition.get("id", 0.0))
    return torch.tensor(
        [
            tactic_id / 32.0,
            float(source_id) / 8.0,
            float(family_id) / 8.0,
        ],
        dtype=torch.float32,
    )


def _safe_mean(values: List[float]) -> float:
    if not values:
        return 0.0
    return float(sum(values) / len(values))


def _build_terminal_scalars(target_terminal: Dict) -> Dict[str, torch.Tensor]:
    terminal_state = target_terminal.get("terminal_state", {})
    outcome = target_terminal.get("episode_outcome", {})
    red_units = terminal_state.get("red_units", [])
    blue_units = terminal_state.get("blue_units", [])

    red_alive = [float(unit.get("alive", 0.0)) for unit in red_units]
    blue_alive = [float(unit.get("alive", 0.0)) for unit in blue_units]
    red_missiles = [float(unit.get("missile_count", 0.0)) / 8.0 for unit in red_units]
    blue_missiles = [float(unit.get("missile_count", 0.0)) / 8.0 for unit in blue_units]
    termination_reason = str(outcome.get("termination_reason", "none"))

    return {
        "terminal_red_win": torch.tensor(float(outcome.get("red_win", 0.0)), dtype=torch.float32),
        "terminal_elapsed_steps": torch.tensor(float(outcome.get("elapsed_steps", 0.0)), dtype=torch.float32),
        "terminal_reason_id": torch.tensor(
            float(TERMINATION_REASON_TO_ID.get(termination_reason, 0)),
            dtype=torch.float32,
        ),
        "terminal_red_alive_ratio": torch.tensor(_safe_mean(red_alive), dtype=torch.float32),
        "terminal_blue_alive_ratio": torch.tensor(_safe_mean(blue_alive), dtype=torch.float32),
        "terminal_red_mean_missile": torch.tensor(_safe_mean(red_missiles), dtype=torch.float32),
        "terminal_blue_mean_missile": torch.tensor(_safe_mean(blue_missiles), dtype=torch.float32),
    }


def derive_effective_termination_flag(record: Dict) -> float:
    target_event = record.get("target_event", {}) if isinstance(record.get("target_event", {}), dict) else {}
    base_flag = 1.0 if bool(target_event.get("termination_flag", False)) else 0.0

    if str(record.get("horizon")) != "terminal":
        return base_flag

    meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
    reason = str(meta.get("target_termination_reason", "")).strip().lower()
    if not reason:
        target_terminal = record.get("target_terminal", {}) if isinstance(record.get("target_terminal", {}), dict) else {}
        episode_outcome = target_terminal.get("episode_outcome", {}) if isinstance(target_terminal.get("episode_outcome", {}), dict) else {}
        reason = str(episode_outcome.get("termination_reason", "none")).strip().lower()

    if reason in NON_DECISIVE_TERMINATION_REASONS:
        return 0.0
    return 1.0 if base_flag >= 0.5 else 0.0


def _build_event_targets(record: Dict) -> Dict[str, torch.Tensor]:
    target_event = record.get("target_event", {})
    if not isinstance(target_event, dict):
        target_event = {}

    termination_value = derive_effective_termination_flag(record)
    event_flags = torch.tensor(
        [
            termination_value
            if key == "termination_flag"
            else (1.0 if bool(target_event.get(key, False)) else 0.0)
            for key in EVENT_FLAG_KEYS
        ],
        dtype=torch.float32,
    )
    event_counts = torch.tensor(
        [float(target_event.get(key, 0.0)) for key in EVENT_COUNT_KEYS],
        dtype=torch.float32,
    )
    return {
        "event_flags": event_flags,
        "event_counts": event_counts,
    }


def _build_reward_targets(target_reward: Dict) -> Dict[str, torch.Tensor]:
    if not isinstance(target_reward, dict):
        target_reward = {}

    reward_stats = {
        side: {
            "clip_low": float(DEFAULT_REWARD_CLIP_LOW),
            "clip_high": float(DEFAULT_REWARD_CLIP_HIGH),
            "mean": float(DEFAULT_REWARD_MEAN),
            "std": float(DEFAULT_REWARD_STD),
        }
        for side in REWARD_KEYS
    }

    def _norm(side: str, value: float) -> float:
        stats = reward_stats[side]
        clipped = min(max(float(value), stats["clip_low"]), stats["clip_high"])
        return (clipped - stats["mean"]) / max(stats["std"], 1e-6)

    return {
        "reward_target": torch.tensor(
            [_norm(side, float(target_reward.get(side, 0.0))) for side in REWARD_KEYS],
            dtype=torch.float32,
        )
    }


def normalize_reward_stats(reward_norm_stats: Optional[Dict[str, Dict[str, Any]]]) -> Dict[str, Dict[str, float]]:
    output: Dict[str, Dict[str, float]] = {}
    for side in REWARD_KEYS:
        source = reward_norm_stats.get(side, {}) if isinstance(reward_norm_stats, dict) else {}
        clip_low = float(source.get("clip_low", DEFAULT_REWARD_CLIP_LOW))
        clip_high = float(source.get("clip_high", DEFAULT_REWARD_CLIP_HIGH))
        if clip_high < clip_low:
            clip_low, clip_high = clip_high, clip_low
        std = float(source.get("std", DEFAULT_REWARD_STD))
        output[side] = {
            "clip_low": clip_low,
            "clip_high": clip_high,
            "mean": float(source.get("mean", DEFAULT_REWARD_MEAN)),
            "std": max(std, 1e-6),
        }
    return output


def denormalize_reward_tensor(reward_tensor: torch.Tensor, reward_norm_stats: Optional[Dict[str, Dict[str, Any]]]) -> torch.Tensor:
    stats = normalize_reward_stats(reward_norm_stats)
    result = reward_tensor.clone()
    for side_index, side in enumerate(REWARD_KEYS):
        side_stats = stats[side]
        result[:, side_index] = result[:, side_index] * side_stats["std"] + side_stats["mean"]
    return result


def build_reward_target_tensor(target_reward: Dict, reward_norm_stats: Optional[Dict[str, Dict[str, Any]]]) -> torch.Tensor:
    stats = normalize_reward_stats(reward_norm_stats)
    values: List[float] = []
    for side in REWARD_KEYS:
        raw_value = float(target_reward.get(side, 0.0)) if isinstance(target_reward, dict) else 0.0
        side_stats = stats[side]
        clipped = min(max(raw_value, side_stats["clip_low"]), side_stats["clip_high"])
        normalized = (clipped - side_stats["mean"]) / side_stats["std"]
        values.append(float(normalized))
    return torch.tensor(values, dtype=torch.float32)


class WorldModelDataset(Dataset):
    def __init__(self, records: List[Dict], reward_norm_stats: Optional[Dict[str, Dict[str, Any]]] = None):
        self.records = records
        self.reward_norm_stats = normalize_reward_stats(reward_norm_stats)

    def __len__(self) -> int:
        return len(self.records)

    def __getitem__(self, idx: int) -> Dict:
        record = self.records[idx]
        state_t = record["state_t"]
        target_state = record["target_state_t_plus_h"]
        target_terminal = record["target_terminal"]
        terminal_state = target_terminal.get("terminal_state", {})

        red_side = _encode_aligned_side(
            state_t.get("red_units", []),
            target_state.get("red_units", []),
            terminal_state.get("red_units", []),
        )
        blue_side = _encode_aligned_side(
            state_t.get("blue_units", []),
            target_state.get("blue_units", []),
            terminal_state.get("blue_units", []),
        )

        terminal_scalars = _build_terminal_scalars(target_terminal)
        horizon_value = record.get("horizon", "terminal")
        if horizon_value not in HORIZON_TO_ID:
            raise KeyError(f"Unsupported horizon value in processed sample: {horizon_value}")

        sample = {
            "red_units": red_side["state_features"],
            "red_unit_types": red_side["type_ids"],
            "red_target": red_side["target_features"],
            "red_terminal": red_side["terminal_features"],
            "red_mask": red_side["state_present"],
            "red_target_mask": red_side["target_present"],
            "red_terminal_mask": red_side["terminal_present"],
            "red_unit_ids": red_side["unit_ids"],
            "blue_units": blue_side["state_features"],
            "blue_unit_types": blue_side["type_ids"],
            "blue_target": blue_side["target_features"],
            "blue_terminal": blue_side["terminal_features"],
            "blue_mask": blue_side["state_present"],
            "blue_target_mask": blue_side["target_present"],
            "blue_terminal_mask": blue_side["terminal_present"],
            "blue_unit_ids": blue_side["unit_ids"],
            "red_tactic": _encode_tactic_condition(record.get("red_tactic_condition", {})),
            "blue_tactic": _encode_tactic_condition(record.get("blue_tactic_condition", {})),
            "red_tactic_condition": record.get("red_tactic_condition", {}),
            "blue_tactic_condition": record.get("blue_tactic_condition", {}),
            "horizon": horizon_value,
            "horizon_id": torch.tensor(HORIZON_TO_ID[horizon_value], dtype=torch.long),
            "is_terminal_horizon": torch.tensor(1.0 if horizon_value == "terminal" else 0.0, dtype=torch.float32),
            "meta": record.get("meta", {}),
        }
        sample.update(terminal_scalars)
        sample.update(_build_event_targets(record))
        sample.update({
            "reward_target": build_reward_target_tensor(record.get("target_reward", {}), self.reward_norm_stats)
        })
        return sample


def _pad_side(
    batch_units: List[torch.Tensor],
    batch_types: List[torch.Tensor],
    batch_targets: List[torch.Tensor],
    batch_terminals: List[torch.Tensor],
    batch_masks: List[torch.Tensor],
    batch_target_masks: List[torch.Tensor],
    batch_terminal_masks: List[torch.Tensor],
) -> Tuple[torch.Tensor, torch.Tensor, torch.Tensor, torch.Tensor, torch.Tensor, torch.Tensor, torch.Tensor]:
    max_len = max(max(int(units.shape[0]), 1) for units in batch_units)
    units = torch.zeros((len(batch_units), max_len, UNIT_FEATURE_DIM), dtype=torch.float32)
    types = torch.zeros((len(batch_units), max_len), dtype=torch.long)
    targets = torch.zeros((len(batch_units), max_len, UNIT_FEATURE_DIM), dtype=torch.float32)
    terminals = torch.zeros((len(batch_units), max_len, UNIT_FEATURE_DIM), dtype=torch.float32)
    masks = torch.zeros((len(batch_units), max_len), dtype=torch.bool)
    target_masks = torch.zeros((len(batch_units), max_len), dtype=torch.bool)
    terminal_masks = torch.zeros((len(batch_units), max_len), dtype=torch.bool)

    for i, side_units in enumerate(batch_units):
        count = int(side_units.shape[0])
        if count <= 0:
            continue
        units[i, :count] = side_units
        types[i, :count] = batch_types[i]
        targets[i, :count] = batch_targets[i]
        terminals[i, :count] = batch_terminals[i]
        masks[i, :count] = batch_masks[i]
        target_masks[i, :count] = batch_target_masks[i]
        terminal_masks[i, :count] = batch_terminal_masks[i]

    return units, types, targets, terminals, masks, target_masks, terminal_masks


def collate_world_model(batch: List[Dict]) -> Dict:
    red_units, red_types, red_target, red_terminal, red_mask, red_target_mask, red_terminal_mask = _pad_side(
        [item["red_units"] for item in batch],
        [item["red_unit_types"] for item in batch],
        [item["red_target"] for item in batch],
        [item["red_terminal"] for item in batch],
        [item["red_mask"] for item in batch],
        [item["red_target_mask"] for item in batch],
        [item["red_terminal_mask"] for item in batch],
    )
    blue_units, blue_types, blue_target, blue_terminal, blue_mask, blue_target_mask, blue_terminal_mask = _pad_side(
        [item["blue_units"] for item in batch],
        [item["blue_unit_types"] for item in batch],
        [item["blue_target"] for item in batch],
        [item["blue_terminal"] for item in batch],
        [item["blue_mask"] for item in batch],
        [item["blue_target_mask"] for item in batch],
        [item["blue_terminal_mask"] for item in batch],
    )

    return {
        "red_units": red_units,
        "red_unit_types": red_types,
        "red_target": red_target,
        "red_terminal": red_terminal,
        "red_mask": red_mask,
        "red_target_mask": red_target_mask,
        "red_terminal_mask": red_terminal_mask,
        "red_unit_ids": [item["red_unit_ids"] for item in batch],
        "blue_units": blue_units,
        "blue_unit_types": blue_types,
        "blue_target": blue_target,
        "blue_terminal": blue_terminal,
        "blue_mask": blue_mask,
        "blue_target_mask": blue_target_mask,
        "blue_terminal_mask": blue_terminal_mask,
        "blue_unit_ids": [item["blue_unit_ids"] for item in batch],
        "red_tactic": torch.stack([item["red_tactic"] for item in batch], dim=0),
        "blue_tactic": torch.stack([item["blue_tactic"] for item in batch], dim=0),
        "red_tactic_condition": [item["red_tactic_condition"] for item in batch],
        "blue_tactic_condition": [item["blue_tactic_condition"] for item in batch],
        "horizon": [item["horizon"] for item in batch],
        "horizon_id": torch.stack([item["horizon_id"] for item in batch], dim=0),
        "is_terminal_horizon": torch.stack([item["is_terminal_horizon"] for item in batch], dim=0),
        "terminal_red_win": torch.stack([item["terminal_red_win"] for item in batch], dim=0),
        "terminal_elapsed_steps": torch.stack([item["terminal_elapsed_steps"] for item in batch], dim=0),
        "terminal_reason_id": torch.stack([item["terminal_reason_id"] for item in batch], dim=0),
        "terminal_red_alive_ratio": torch.stack([item["terminal_red_alive_ratio"] for item in batch], dim=0),
        "terminal_blue_alive_ratio": torch.stack([item["terminal_blue_alive_ratio"] for item in batch], dim=0),
        "terminal_red_mean_missile": torch.stack([item["terminal_red_mean_missile"] for item in batch], dim=0),
        "terminal_blue_mean_missile": torch.stack([item["terminal_blue_mean_missile"] for item in batch], dim=0),
        "event_flags": torch.stack([item["event_flags"] for item in batch], dim=0),
        "event_counts": torch.stack([item["event_counts"] for item in batch], dim=0),
        "reward_target": torch.stack([item["reward_target"] for item in batch], dim=0),
        "meta": [item["meta"] for item in batch],
    }
