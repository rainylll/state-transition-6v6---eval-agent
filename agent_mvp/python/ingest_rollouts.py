import argparse
import random
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Set, Tuple, Union

from data_io import read_jsonl, write_json, write_jsonl

HorizonValue = Union[int, str]
SUPPORTED_HORIZONS: Tuple[HorizonValue, ...] = (1, 5, 10, 20, "terminal")
SUPPORTED_OOD_AXES = (
    "none",
    "force_size_key",
    "red_attr_bucket",
    "blue_attr_bucket",
    "any_attr_bucket",
    "tactic_pair_key",
)
REQUIRED_TOP_LEVEL_FIELDS = (
    "task_id",
    "episode_id",
    "step",
    "sim_time_s",
    "red_tactic_condition",
    "blue_tactic_condition",
    "state",
    "next_state",
    "done",
    "termination_reason",
    "episode_outcome",
)
REQUIRED_STATE_FIELDS = ("red_units", "blue_units")
REQUIRED_UNIT_FIELDS = (
    "unit_id",
    "type_id",
    "alive",
    "missile_count",
    "lon",
    "lat",
    "alt_m",
    "speed_mps",
    "heading_deg",
)
REQUIRED_TACTIC_FIELDS = ("source", "id")
REQUIRED_OUTCOME_FIELDS = (
    "red_win",
    "elapsed_steps",
    "termination_reason",
    "red_alive_final",
    "blue_alive_final",
    "red_missile_final",
    "blue_missile_final",
)
EPISODE_META_CONSISTENCY_FIELDS = (
    "sampling_plan",
    "tactic_pair_key",
    "tactic_combo_key",
    "force_size_key",
    "red_attr_bucket",
    "blue_attr_bucket",
    "scenario_key",
    "split_group_key",
)
EVENT_BINARY_KEYS = (
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
NON_DECISIVE_TERMINATION_REASONS = {
    "none",
    "safety_limit",
    "timeout",
    "time_limit",
}
VIEW_SIDE_LABELS = ("red", "blue")
OPPOSING_VIEW_SIDE = {"red": "blue", "blue": "red"}


def derive_effective_terminal_reason(meta: Dict, target_terminal: Dict) -> str:
    reason = str(meta.get("target_termination_reason", "")).strip().lower()
    if reason:
        return reason
    outcome = target_terminal.get("episode_outcome", {}) if isinstance(target_terminal, dict) else {}
    return str(outcome.get("termination_reason", "none")).strip().lower()


def derive_terminal_self_view_roles(sample: Dict) -> Dict[str, str]:
    horizon = str(sample.get("horizon"))
    target_event = sample.get("target_event", {}) if isinstance(sample.get("target_event", {}), dict) else {}
    output: Dict[str, str] = {}
    for side in VIEW_SIDE_LABELS:
        if horizon != "terminal":
            output[side] = "other_terminal"
            continue
        other_side = OPPOSING_VIEW_SIDE[side]
        self_first_kill = bool(target_event.get(f"{side}_first_kill_flag", False))
        other_first_kill = bool(target_event.get(f"{other_side}_first_kill_flag", False))
        self_objective = bool(target_event.get(f"{side}_objective_complete_flag", False))
        other_objective = bool(target_event.get(f"{other_side}_objective_complete_flag", False))
        if self_first_kill:
            output[side] = "self_first_kill_terminal"
        elif other_first_kill or self_objective or other_objective:
            output[side] = "non_self_terminal_critical"
        else:
            output[side] = "other_terminal"
    return output


def build_target_event_contract(sample: Dict) -> Dict:
    horizon = str(sample.get("horizon"))
    target_event = sample.get("target_event", {}) if isinstance(sample.get("target_event", {}), dict) else {}
    meta = sample.get("meta", {}) if isinstance(sample.get("meta", {}), dict) else {}
    target_terminal = sample.get("target_terminal", {}) if isinstance(sample.get("target_terminal", {}), dict) else {}

    red_first_kill = bool(target_event.get("red_first_kill_flag", False))
    blue_first_kill = bool(target_event.get("blue_first_kill_flag", False))
    red_objective = bool(target_event.get("red_objective_complete_flag", False))
    blue_objective = bool(target_event.get("blue_objective_complete_flag", False))
    effective_reason = derive_effective_terminal_reason(meta, target_terminal)
    effective_terminal = bool(horizon == "terminal" and effective_reason not in NON_DECISIVE_TERMINATION_REASONS and effective_reason)

    terminal_role = "non_terminal"
    blue_supervision_mask = 1
    if horizon == "terminal":
        if blue_first_kill:
            terminal_role = "blue_first_kill"
            blue_supervision_mask = 1
        elif red_first_kill:
            terminal_role = "red_first_kill"
            blue_supervision_mask = 0
        elif blue_objective:
            terminal_role = "blue_objective_only"
            blue_supervision_mask = 0
        elif not red_first_kill and not blue_first_kill and not red_objective and not blue_objective:
            terminal_role = "no_first_kill_remaining" if not effective_terminal else "non_decisive"
            blue_supervision_mask = 0
        else:
            terminal_role = "non_decisive"
            blue_supervision_mask = 1

    return {
        "blue_first_kill_flag": 1 if blue_first_kill else 0,
        "blue_first_kill_supervision_mask": int(blue_supervision_mask),
        "terminal_critical_role": terminal_role,
        "terminal_self_view_roles": derive_terminal_self_view_roles(sample),
    }


def _try_parse_tactic_id(raw: object) -> Union[int, None]:
    if raw is None:
        return None
    if isinstance(raw, bool):
        return int(raw)
    if isinstance(raw, (int, float)):
        return int(raw)
    if isinstance(raw, str):
        text = raw.strip()
        if not text:
            return None
        try:
            return int(text)
        except ValueError:
            digits: List[str] = []
            current: List[str] = []
            for ch in text:
                if ch.isdigit() or (ch == "-" and not current):
                    current.append(ch)
                elif current:
                    digits.append("".join(current))
                    current = []
            if current:
                digits.append("".join(current))
            if digits:
                try:
                    return int(digits[-1])
                except ValueError:
                    return None
    return None


def normalize_tactic_condition(condition: Dict) -> Dict:
    normalized = dict(condition)
    source = normalized.get("source", "shared_tactic_id")
    normalized["source"] = str(source)

    family_default = "legacy_shared_tactic" if normalized["source"] == "legacy_tactic_id" else "rule"
    normalized["family"] = str(normalized.get("family", family_default))

    if "id" not in normalized:
        raise ValueError("Tactic condition is missing required field: id")
    normalized["id"] = str(normalized["id"])

    params = normalized.get("params", {})
    if not isinstance(params, dict):
        params = {}

    tactic_id = (
        _try_parse_tactic_id(params.get("tactic_id"))
        or _try_parse_tactic_id(normalized.get("tactic_id"))
        or _try_parse_tactic_id(normalized.get("id"))
    )
    if tactic_id is not None:
        params["tactic_id"] = int(tactic_id)
    normalized["params"] = params

    if "side" in normalized:
        normalized["side"] = str(normalized["side"])
    return normalized


def build_fallback_sampling_meta(row: Dict) -> Dict:
    red_condition = normalize_tactic_condition(row["red_tactic_condition"])
    blue_condition = normalize_tactic_condition(row["blue_tactic_condition"])
    red_units = row["state"]["red_units"]
    blue_units = row["state"]["blue_units"]
    red_force_size = len(red_units)
    blue_force_size = len(blue_units)
    force_size_key = f"{red_force_size}v{blue_force_size}"
    tactic_pair_key = f"{red_condition['family']}-{blue_condition['family']}"
    tactic_combo_key = f"{red_condition['id']}__{blue_condition['id']}"
    scenario_key = f"{force_size_key}__{tactic_combo_key}"
    split_group_key = str(row.get("split_group_key") or scenario_key)
    return {
        "sampling_plan": "rollout_fallback_v1",
        "tactic_pair_key": tactic_pair_key,
        "tactic_combo_key": tactic_combo_key,
        "red_tactic_family": red_condition["family"],
        "blue_tactic_family": blue_condition["family"],
        "red_tactic_id": red_condition.get("params", {}).get("tactic_id", 0),
        "blue_tactic_id": blue_condition.get("params", {}).get("tactic_id", 0),
        "force_size_key": force_size_key,
        "red_force_size": red_force_size,
        "blue_force_size": blue_force_size,
        "red_attr_bucket": "unknown",
        "blue_attr_bucket": "unknown",
        "scenario_key": scenario_key,
        "split_group_key": split_group_key,
    }


def normalize_sampling_meta(raw_meta: object, row: Dict) -> Dict:
    fallback = build_fallback_sampling_meta(row)
    if not isinstance(raw_meta, dict):
        return fallback

    normalized = dict(raw_meta)
    for key, fallback_value in fallback.items():
        value = normalized.get(key, fallback_value)
        if isinstance(fallback_value, str):
            normalized[key] = str(value)
        elif isinstance(fallback_value, int):
            try:
                normalized[key] = int(value)
            except (TypeError, ValueError):
                normalized[key] = fallback_value
        else:
            normalized[key] = value

    split_group_key = row.get("split_group_key", normalized.get("split_group_key", normalized["scenario_key"]))
    normalized["split_group_key"] = str(split_group_key)
    return normalized


def default_event_payload() -> Dict:
    payload = {key: False for key in EVENT_BINARY_KEYS}
    payload.update({key: 0 for key in EVENT_COUNT_KEYS})
    return payload


def normalize_event_payload(raw_event: object) -> Dict:
    normalized = default_event_payload()
    if not isinstance(raw_event, dict):
        return normalized

    for key in EVENT_BINARY_KEYS:
        normalized[key] = bool(raw_event.get(key, False))
    for key in EVENT_COUNT_KEYS:
        try:
            normalized[key] = int(raw_event.get(key, 0))
        except (TypeError, ValueError):
            normalized[key] = 0
    return normalized


def normalize_reward_payload(raw_reward: object) -> Dict:
    normalized = {"red": 0.0, "blue": 0.0}
    if not isinstance(raw_reward, dict):
        return normalized
    for side in ("red", "blue"):
        try:
            normalized[side] = float(raw_reward.get(side, 0.0))
        except (TypeError, ValueError):
            normalized[side] = 0.0
    return normalized


def parse_horizons(raw: str) -> List[HorizonValue]:
    values: List[HorizonValue] = []
    for token in raw.split(","):
        item = token.strip().lower()
        if not item:
            continue
        if item == "terminal":
            values.append("terminal")
            continue
        try:
            values.append(int(item))
        except ValueError as exc:
            raise ValueError(f"Unsupported horizon token: {token}") from exc

    if not values:
        raise ValueError("At least one horizon must be provided.")

    invalid = [value for value in values if value not in SUPPORTED_HORIZONS]
    if invalid:
        raise ValueError(f"Unsupported horizons: {invalid}. Supported: {SUPPORTED_HORIZONS}")
    return values


def split_ids(
    ids: Sequence[str],
    train_ratio: float,
    val_ratio: float,
    seed: int,
) -> Dict[str, List[str]]:
    values = list(ids)
    rng = random.Random(seed)
    rng.shuffle(values)

    n_total = len(values)
    if n_total == 0:
        return {"train": [], "val": [], "test": []}
    if n_total == 1:
        return {"train": values, "val": [], "test": []}
    if n_total == 2:
        return {"train": values[:1], "val": [], "test": values[1:]}

    n_train = max(1, int(n_total * train_ratio))
    n_val = max(1, int(n_total * val_ratio))
    if n_train + n_val >= n_total:
        n_val = max(1, n_total - n_train - 1)
    n_test = n_total - n_train - n_val
    if n_test <= 0:
        n_test = 1
        if n_train > n_val:
            n_train -= 1
        else:
            n_val -= 1

    return {
        "train": values[:n_train],
        "val": values[n_train:n_train + n_val],
        "test": values[n_train + n_val:],
    }


def _require_fields(payload: Dict, required_fields: Sequence[str], context: str) -> None:
    missing = [field for field in required_fields if field not in payload]
    if missing:
        raise ValueError(f"Missing fields in {context}: {missing}")


def _validate_units(units: Iterable[Dict], context: str) -> None:
    for unit_index, unit in enumerate(units):
        if not isinstance(unit, dict):
            raise ValueError(f"Expected dict unit at {context}[{unit_index}]")
        _require_fields(unit, REQUIRED_UNIT_FIELDS, f"{context}[{unit_index}]")


def validate_rollout_row(row: Dict, row_index: int) -> None:
    if not isinstance(row, dict):
        raise ValueError(f"Rollout row {row_index} is not a JSON object")

    _require_fields(row, REQUIRED_TOP_LEVEL_FIELDS, f"rollout row {row_index}")
    for state_key in ("state", "next_state"):
        state = row[state_key]
        if not isinstance(state, dict):
            raise ValueError(f"Expected dict at rollout row {row_index}.{state_key}")
        _require_fields(state, REQUIRED_STATE_FIELDS, f"rollout row {row_index}.{state_key}")
        for side_key in REQUIRED_STATE_FIELDS:
            if not isinstance(state[side_key], list):
                raise ValueError(f"Expected list at rollout row {row_index}.{state_key}.{side_key}")
            _validate_units(state[side_key], f"rollout row {row_index}.{state_key}.{side_key}")

    for tactic_key in ("red_tactic_condition", "blue_tactic_condition"):
        condition = row[tactic_key]
        if not isinstance(condition, dict):
            raise ValueError(f"Expected dict at rollout row {row_index}.{tactic_key}")
        _require_fields(condition, REQUIRED_TACTIC_FIELDS, f"rollout row {row_index}.{tactic_key}")
        row[tactic_key] = normalize_tactic_condition(condition)

    outcome = row["episode_outcome"]
    if not isinstance(outcome, dict):
        raise ValueError(f"Expected dict at rollout row {row_index}.episode_outcome")
    _require_fields(outcome, REQUIRED_OUTCOME_FIELDS, f"rollout row {row_index}.episode_outcome")

    row["event"] = normalize_event_payload(row.get("event"))
    row["reward_step"] = normalize_reward_payload(row.get("reward_step"))
    row["reward_cumulative"] = normalize_reward_payload(row.get("reward_cumulative"))
    row["sampling_meta"] = normalize_sampling_meta(row.get("sampling_meta"), row)
    row["split_group_key"] = str(row["sampling_meta"]["split_group_key"])


def group_rows_by_episode(rows: Iterable[Dict]) -> Dict[str, List[Dict]]:
    grouped: Dict[str, List[Dict]] = defaultdict(list)
    for row_index, row in enumerate(rows):
        validate_rollout_row(row, row_index)
        episode_id = str(row.get("episode_id") or row.get("task_id") or "")
        if not episode_id:
            continue
        grouped[episode_id].append(row)

    for episode_rows in grouped.values():
        episode_rows.sort(key=lambda item: int(item.get("step", 0)))
    return grouped


def build_episode_info(episode_id: str, rows: List[Dict]) -> Dict:
    first = rows[0]
    sampling_meta = dict(first["sampling_meta"])
    split_group_key = str(first["split_group_key"])
    for row in rows[1:]:
        if str(row["split_group_key"]) != split_group_key:
            raise ValueError(f"Episode {episode_id} mixes multiple split_group_key values.")
        row_meta = row["sampling_meta"]
        for field in EPISODE_META_CONSISTENCY_FIELDS:
            if row_meta.get(field) != sampling_meta.get(field):
                raise ValueError(
                    f"Episode {episode_id} has inconsistent sampling_meta field '{field}': "
                    f"{sampling_meta.get(field)!r} vs {row_meta.get(field)!r}"
                )

    return {
        "episode_id": episode_id,
        "task_id": str(first.get("task_id", episode_id)),
        "split_group_key": split_group_key,
        "sampling_meta": sampling_meta,
        "num_rollout_rows": len(rows),
    }


def build_episode_infos(grouped_rows: Dict[str, List[Dict]]) -> Dict[str, Dict]:
    return {
        episode_id: build_episode_info(episode_id, rows)
        for episode_id, rows in grouped_rows.items()
    }


def infer_export_stride_steps(rows: Sequence[Dict]) -> int:
    step_values = [int(row["step"]) for row in rows]
    positive_deltas = [curr - prev for prev, curr in zip(step_values, step_values[1:]) if (curr - prev) > 0]
    if positive_deltas:
        return Counter(positive_deltas).most_common(1)[0][0]
    if step_values:
        return max(step_values[0], 1)
    return 1


def build_episode_views(rows: List[Dict]) -> Tuple[List[Dict], List[int], List[float], Dict, int]:
    if not rows:
        return [], [], [], {}, 1

    states: List[Dict] = [rows[0]["state"]]
    step_values: List[int] = [0]
    sim_times: List[float] = [0.0]

    for row in rows:
        states.append(row["next_state"])
        step_values.append(int(row["step"]))
        sim_times.append(float(row["sim_time_s"]))

    terminal_row = rows[-1]
    target_terminal = {
        "episode_outcome": terminal_row["episode_outcome"],
        "terminal_state": states[-1],
        "terminal_step": step_values[-1],
        "terminal_sim_time_s": sim_times[-1],
    }
    export_stride_steps = infer_export_stride_steps(rows)
    return states, step_values, sim_times, target_terminal, export_stride_steps


def aggregate_event_window(window_rows: Sequence[Dict]) -> Dict:
    aggregated = default_event_payload()
    for row in window_rows:
        event = normalize_event_payload(row.get("event"))
        for key in EVENT_BINARY_KEYS:
            aggregated[key] = aggregated[key] or bool(event[key])
        for key in EVENT_COUNT_KEYS:
            aggregated[key] += int(event[key])
    return aggregated


def aggregate_reward_window(window_rows: Sequence[Dict]) -> Dict:
    reward = {"red": 0.0, "blue": 0.0}
    for row in window_rows:
        step_reward = normalize_reward_payload(row.get("reward_step"))
        reward["red"] += float(step_reward["red"])
        reward["blue"] += float(step_reward["blue"])
    return reward


def make_sample(
    rows: List[Dict],
    states: List[Dict],
    step_values: List[int],
    sim_times: List[float],
    target_terminal: Dict,
    export_stride_steps: int,
    t_index: int,
    horizon: HorizonValue,
    episode_info: Dict,
    split_name: str,
) -> Dict:
    target_index = len(states) - 1 if horizon == "terminal" else t_index + int(horizon)
    row_meta = rows[min(t_index, len(rows) - 1)]
    target_row = rows[target_index - 1] if target_index > 0 else row_meta
    sampling_meta = dict(episode_info["sampling_meta"])
    event_window_rows = rows[t_index:target_index]

    sample = {
        "state_t": states[t_index],
        "red_tactic_condition": row_meta["red_tactic_condition"],
        "blue_tactic_condition": row_meta["blue_tactic_condition"],
        "horizon": horizon,
        "target_state_t_plus_h": states[target_index],
        "target_event": aggregate_event_window(event_window_rows),
        "target_reward": aggregate_reward_window(event_window_rows),
        "target_terminal": target_terminal,
        "meta": {
            "task_id": row_meta["task_id"],
            "episode_id": row_meta.get("episode_id", row_meta["task_id"]),
            "split_name": split_name,
            "split_group_key": episode_info["split_group_key"],
            "sampling_meta": sampling_meta,
            "t_index": t_index,
            "target_index": target_index,
            "state_step": step_values[t_index],
            "target_step": step_values[target_index],
            "state_sim_time_s": sim_times[t_index],
            "target_sim_time_s": sim_times[target_index],
            "episode_export_stride_steps": export_stride_steps,
            "episode_num_views": len(states),
            "episode_elapsed_steps": int(target_terminal["episode_outcome"]["elapsed_steps"]),
            "target_done": bool(target_index == (len(states) - 1)),
            "target_termination_reason": target_row.get("termination_reason", "none"),
            "horizon_unit": "export_view",
        },
    }
    sample["target_event_contract"] = build_target_event_contract(sample)
    return sample


def expand_episode(rows: List[Dict], horizons: Sequence[HorizonValue], episode_info: Dict, split_name: str) -> List[Dict]:
    states, step_values, sim_times, target_terminal, export_stride_steps = build_episode_views(rows)
    if len(states) <= 1:
        return []

    samples: List[Dict] = []
    last_index = len(states) - 1
    for t_index in range(last_index):
        for horizon in horizons:
            if horizon == "terminal":
                samples.append(
                    make_sample(
                        rows,
                        states,
                        step_values,
                        sim_times,
                        target_terminal,
                        export_stride_steps,
                        t_index,
                        horizon,
                        episode_info,
                        split_name,
                    )
                )
                continue

            target_index = t_index + int(horizon)
            if target_index > last_index:
                continue
            samples.append(
                make_sample(
                    rows,
                    states,
                    step_values,
                    sim_times,
                    target_terminal,
                    export_stride_steps,
                    t_index,
                    horizon,
                    episode_info,
                    split_name,
                )
            )
    return samples


def build_group_infos(episode_infos: Dict[str, Dict]) -> Dict[str, Dict]:
    grouped: Dict[str, Dict] = {}
    for episode_id, episode_info in episode_infos.items():
        split_group_key = str(episode_info["split_group_key"])
        entry = grouped.setdefault(
            split_group_key,
            {
                "split_group_key": split_group_key,
                "episode_ids": [],
                "sampling_meta": episode_info["sampling_meta"],
            },
        )
        entry["episode_ids"].append(episode_id)
    return grouped


def matches_ood_axis(sampling_meta: Dict, ood_axis: str, ood_values: Set[str]) -> bool:
    if ood_axis == "none" or not ood_values:
        return False
    if ood_axis == "any_attr_bucket":
        return (
            str(sampling_meta.get("red_attr_bucket", "")) in ood_values
            or str(sampling_meta.get("blue_attr_bucket", "")) in ood_values
        )
    return str(sampling_meta.get(ood_axis, "")) in ood_values


def assign_group_splits(
    group_infos: Dict[str, Dict],
    train_ratio: float,
    val_ratio: float,
    seed: int,
    ood_axis: str,
    ood_values: Set[str],
) -> Dict[str, List[str]]:
    ood_groups: List[str] = []
    seen_groups: List[str] = []
    for group_key, group_info in sorted(group_infos.items()):
        if matches_ood_axis(group_info["sampling_meta"], ood_axis, ood_values):
            ood_groups.append(group_key)
        else:
            seen_groups.append(group_key)

    seen_split = split_ids(seen_groups, train_ratio, val_ratio, seed)
    return {
        "train": seen_split["train"],
        "val": seen_split["val"],
        "test": seen_split["test"],
        "ood": ood_groups,
    }


def build_split_summary(
    split_name: str,
    split_group_keys: List[str],
    group_infos: Dict[str, Dict],
    split_records: List[Dict],
    skipped_episodes: int,
) -> Dict:
    split_episode_ids: List[str] = []
    tactic_pair_keys: Set[str] = set()
    force_size_keys: Set[str] = set()
    red_attr_buckets: Set[str] = set()
    blue_attr_buckets: Set[str] = set()
    attr_bucket_pairs: Set[str] = set()

    for group_key in split_group_keys:
        group_info = group_infos[group_key]
        split_episode_ids.extend(group_info["episode_ids"])
        sampling_meta = group_info["sampling_meta"]
        tactic_pair_keys.add(str(sampling_meta.get("tactic_pair_key", "unknown")))
        force_size_keys.add(str(sampling_meta.get("force_size_key", "unknown")))
        red_attr = str(sampling_meta.get("red_attr_bucket", "unknown"))
        blue_attr = str(sampling_meta.get("blue_attr_bucket", "unknown"))
        red_attr_buckets.add(red_attr)
        blue_attr_buckets.add(blue_attr)
        attr_bucket_pairs.add(f"{red_attr}__{blue_attr}")

    return {
        "groups": len(split_group_keys),
        "split_group_keys": split_group_keys,
        "episodes": len(split_episode_ids),
        "episode_ids": sorted(split_episode_ids),
        "samples": len(split_records),
        "skipped_episodes": skipped_episodes,
        "tactic_pair_keys": sorted(tactic_pair_keys),
        "force_size_keys": sorted(force_size_keys),
        "red_attr_buckets": sorted(red_attr_buckets),
        "blue_attr_buckets": sorted(blue_attr_buckets),
        "attr_bucket_pairs": sorted(attr_bucket_pairs),
    }


def build_group_leak_report(splits: Dict[str, List[str]]) -> Dict:
    report: Dict[str, List[str]] = {}
    split_names = list(splits.keys())
    leak_detected = False
    for i, left in enumerate(split_names):
        for right in split_names[i + 1 :]:
            overlap = sorted(set(splits[left]) & set(splits[right]))
            report[f"{left}__{right}"] = overlap
            if overlap:
                leak_detected = True
    report["group_leak_detected"] = leak_detected
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description="Ingest rollout JSONL into world-model any-step samples.")
    parser.add_argument(
        "--rollouts-path",
        type=Path,
        default=Path("agent_mvp/data_real/raw/rollouts.jsonl"),
    )
    parser.add_argument(
        "--out-dir",
        type=Path,
        default=Path("agent_mvp/data_world_model"),
    )
    parser.add_argument("--train-ratio", type=float, default=0.7)
    parser.add_argument("--val-ratio", type=float, default=0.15)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument(
        "--horizons",
        type=str,
        default="1,5,10,20,terminal",
        help="Comma-separated horizons. Supported: 1,5,10,20,terminal",
    )
    parser.add_argument(
        "--ood-axis",
        type=str,
        default="force_size_key",
        choices=SUPPORTED_OOD_AXES,
        help="Which sampling_meta axis to reserve as OOD.",
    )
    parser.add_argument(
        "--ood-values",
        type=str,
        default="6v6",
        help="Comma-separated values for the selected OOD axis.",
    )
    args = parser.parse_args()

    horizons = parse_horizons(args.horizons)
    ood_values = {token.strip() for token in args.ood_values.split(",") if token.strip()}
    rows = read_jsonl(args.rollouts_path)
    grouped_rows = group_rows_by_episode(rows)
    episode_infos = build_episode_infos(grouped_rows)
    group_infos = build_group_infos(episode_infos)
    split_group_assignments = assign_group_splits(
        group_infos,
        args.train_ratio,
        args.val_ratio,
        args.seed,
        args.ood_axis,
        ood_values,
    )

    processed_dir = args.out_dir / "processed"
    split_records_map: Dict[str, List[Dict]] = {}
    summary = {
        "rollouts_path": str(args.rollouts_path),
        "horizons": list(horizons),
        "num_rollout_rows": len(rows),
        "num_episodes": len(grouped_rows),
        "num_split_groups": len(group_infos),
        "ood_axis": args.ood_axis,
        "ood_values": sorted(ood_values),
        "observed_top_level_fields": sorted(list(rows[0].keys())) if rows else [],
        "group_leak_report": {},
        "splits": {},
    }

    for split_name in ("train", "val", "test", "ood"):
        split_group_keys = split_group_assignments.get(split_name, [])
        split_records: List[Dict] = []
        skipped_episodes = 0
        for split_group_key in split_group_keys:
            group_info = group_infos[split_group_key]
            for episode_id in group_info["episode_ids"]:
                episode_rows = grouped_rows[episode_id]
                episode_info = episode_infos[episode_id]
                expanded = expand_episode(episode_rows, horizons, episode_info, split_name)
                if not expanded:
                    skipped_episodes += 1
                    continue
                split_records.extend(expanded)

        split_records_map[split_name] = split_records
        write_jsonl(processed_dir / f"{split_name}.jsonl", split_records)
        summary["splits"][split_name] = build_split_summary(
            split_name,
            split_group_keys,
            group_infos,
            split_records,
            skipped_episodes,
        )
        print(
            f"{split_name}: groups={summary['splits'][split_name]['groups']} "
            f"episodes={summary['splits'][split_name]['episodes']} "
            f"samples={summary['splits'][split_name]['samples']} "
            f"skipped={skipped_episodes}"
        )

    summary["group_leak_report"] = build_group_leak_report(split_group_assignments)
    write_json(processed_dir / "summary.json", summary)
    print(f"Wrote world-model processed data to: {processed_dir}")


if __name__ == "__main__":
    main()
