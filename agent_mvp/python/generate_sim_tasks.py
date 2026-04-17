import argparse
import copy
import json
import math
import random
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


# ==========================================================
# USER_CONFIG_ZONE (edit this section only for quick custom batches)
# - This remains the simple single-config path used by the baseline flow.
# - Phase 3 adds an optional combinatorial sampling mode via CLI, without
#   breaking this legacy default behavior.
# ==========================================================
USER_CONFIG_ZONE = {
    "task_count": 1000,
    "seed": 2026,
    "tactic_id": 1,
    # Optional Phase 2.5 fields. If omitted, generator falls back to shared legacy tactic_id.
    # "red_tactic_condition": {"source": "task_config", "family": "rule", "id": "red_rule_1", "params": {"tactic_id": 1}},
    # "blue_tactic_condition": {"source": "task_config", "family": "rl", "id": "blue_rl_2", "params": {"tactic_id": 2}},
    "red_template": [
        {"type_id": 0.0, "speed": 300.0, "sensor": 120.0, "alive": 1.0},
        {"type_id": 0.0, "speed": 300.0, "sensor": 120.0, "alive": 1.0},
    ],
    "blue_template": [
        {"type_id": 0.0, "speed": 300.0, "sensor": 120.0, "alive": 1.0},
    ],
    "battlefield": {
        "red_base_lon": 116.200000,
        "red_base_lat": 21.577106,
        "blue_base_lon": 119.800000,
        "blue_base_lat": 21.577106,
        "square_side_km": 600.0,
        "edge_margin_km": 60.0,
        "center_gap_km": 30.0,
    },
    "ranges": {
        "red_missile": [0, 6],
        "blue_missile": [0, 6],
    },
}

TACTIC_PAIR_TO_FAMILIES = {
    "rule-rule": ("rule", "rule"),
    "rule-rl": ("rule", "rl"),
    "rl-rule": ("rl", "rule"),
    "rl-rl": ("rl", "rl"),
}

FORCE_SIZE_LIBRARY = {
    "2v1": {"red_count": 2, "blue_count": 1},
    "2v2": {"red_count": 2, "blue_count": 2},
    "3v2": {"red_count": 3, "blue_count": 2},
    "6v6": {"red_count": 6, "blue_count": 6},
}

ATTRIBUTE_BUCKET_LIBRARY = {
    "slow_short_light": {
        "speed": 260.0,
        "sensor": 80.0,
        "missile_range": [0, 2],
        "type_sequence": [0.0],
    },
    "balanced_medium": {
        "speed": 300.0,
        "sensor": 120.0,
        "missile_range": [2, 4],
        "type_sequence": [0.0],
    },
    "fast_long_heavy": {
        "speed": 340.0,
        "sensor": 160.0,
        "missile_range": [4, 6],
        "type_sequence": [0.0],
    },
    "fast_short_striker": {
        "speed": 360.0,
        "sensor": 90.0,
        "missile_range": [2, 3],
        "type_sequence": [0.0],
    },
}

DEFAULT_TACTIC_PAIR_KEYS = ("rule-rule", "rule-rl", "rl-rule", "rl-rl")
DEFAULT_FORCE_SIZE_KEYS = ("2v1", "2v2", "3v2", "6v6")
DEFAULT_ATTR_BUCKET_KEYS = ("slow_short_light", "balanced_medium", "fast_long_heavy")
DEFAULT_RULE_TACTIC_IDS = (1, 3)
DEFAULT_RL_TACTIC_IDS = (2, 4)


def _try_parse_tactic_id(raw: object) -> Optional[int]:
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
            digit_runs: List[str] = []
            current: List[str] = []
            for ch in text:
                if ch.isdigit() or (ch == "-" and not current):
                    current.append(ch)
                elif current:
                    digit_runs.append("".join(current))
                    current = []
            if current:
                digit_runs.append("".join(current))
            if digit_runs:
                try:
                    return int(digit_runs[-1])
                except ValueError:
                    return None
    return None


def _parse_csv_tokens(raw: str) -> List[str]:
    return [token.strip() for token in raw.split(",") if token.strip()]


def _parse_csv_ints(raw: str) -> List[int]:
    values: List[int] = []
    for token in _parse_csv_tokens(raw):
        values.append(int(token))
    return values


def _build_legacy_tactic_condition(tactic_id: int) -> Dict[str, object]:
    return {
        "source": "legacy_tactic_id",
        "family": "legacy_shared_tactic",
        "id": f"tactic_{int(tactic_id)}",
        "params": {"tactic_id": int(tactic_id)},
    }


def _normalize_tactic_condition(raw: object, fallback_tactic_id: int) -> Dict[str, object]:
    if not isinstance(raw, dict):
        return _build_legacy_tactic_condition(fallback_tactic_id)

    normalized = copy.deepcopy(raw)
    params = normalized.get("params", {})
    if not isinstance(params, dict):
        params = {}

    tactic_id = (
        _try_parse_tactic_id(params.get("tactic_id"))
        or _try_parse_tactic_id(normalized.get("tactic_id"))
        or _try_parse_tactic_id(normalized.get("id"))
        or int(fallback_tactic_id)
    )
    params["tactic_id"] = int(tactic_id)

    normalized["source"] = str(normalized.get("source", "task_config"))
    normalized["family"] = str(normalized.get("family", "rule"))
    normalized["id"] = str(normalized.get("id", f"tactic_{int(tactic_id)}"))
    normalized["params"] = params
    return normalized


def _resolve_side_tactic_condition(cfg: Dict[str, object], side: str) -> Dict[str, object]:
    fallback_tactic_id = int(cfg.get("tactic_id", 0))
    return _normalize_tactic_condition(cfg.get(f"{side}_tactic_condition"), fallback_tactic_id)


def _apply_side_cli_override(cfg: Dict[str, object], args: argparse.Namespace, side: str) -> None:
    source = getattr(args, f"{side}_source")
    family = getattr(args, f"{side}_family")
    condition_id = getattr(args, f"{side}_id")
    tactic_id = getattr(args, f"{side}_tactic_id")

    if source is None and family is None and condition_id is None and tactic_id is None:
        return

    condition = copy.deepcopy(cfg.get(f"{side}_tactic_condition", {}))
    if not isinstance(condition, dict):
        condition = {}

    params = condition.get("params", {})
    if not isinstance(params, dict):
        params = {}

    if source is not None:
        condition["source"] = source
    if family is not None:
        condition["family"] = family
    if condition_id is not None:
        condition["id"] = condition_id
    if tactic_id is not None:
        params["tactic_id"] = int(tactic_id)

    condition["params"] = params
    cfg[f"{side}_tactic_condition"] = condition


def _apply_cli_tactic_overrides(cfg: Dict[str, object], args: argparse.Namespace) -> Dict[str, object]:
    updated = copy.deepcopy(cfg)
    _apply_side_cli_override(updated, args, "red")
    _apply_side_cli_override(updated, args, "blue")
    return updated


def _build_catalog_tactic_condition(source: str, family: str, tactic_id: int) -> Dict[str, object]:
    return {
        "source": source,
        "family": family,
        "id": f"{family}_tactic_{int(tactic_id)}",
        "params": {"tactic_id": int(tactic_id)},
    }


def _build_tactic_catalog(args: argparse.Namespace) -> Dict[str, List[Dict[str, object]]]:
    rule_ids = _parse_csv_ints(args.rule_tactic_ids)
    rl_ids = _parse_csv_ints(args.rl_tactic_ids)
    catalog = {
        "rule": [_build_catalog_tactic_condition(args.catalog_source, "rule", tactic_id) for tactic_id in rule_ids],
        "rl": [_build_catalog_tactic_condition(args.catalog_source, "rl", tactic_id) for tactic_id in rl_ids],
    }
    for family, conditions in catalog.items():
        if not conditions:
            raise ValueError(f"Tactic family '{family}' has an empty catalog.")
    return catalog


def _build_team_template(unit_count: int, attr_bucket_key: str) -> List[Dict[str, float]]:
    bucket = ATTRIBUTE_BUCKET_LIBRARY[attr_bucket_key]
    type_sequence = bucket.get("type_sequence", [0.0])
    if not isinstance(type_sequence, list) or not type_sequence:
        type_sequence = [0.0]

    template: List[Dict[str, float]] = []
    for index in range(unit_count):
        template.append(
            {
                "type_id": float(type_sequence[index % len(type_sequence)]),
                "speed": float(bucket["speed"]),
                "sensor": float(bucket["sensor"]),
                "alive": 1.0,
            }
        )
    return template


def _build_simple_sampling_meta(
    red_tactic_condition: Dict[str, object],
    blue_tactic_condition: Dict[str, object],
    red_template: Sequence[Dict[str, float]],
    blue_template: Sequence[Dict[str, float]],
) -> Dict[str, object]:
    force_size_key = f"{len(red_template)}v{len(blue_template)}"
    return {
        "sampling_plan": "simple_v1",
        "tactic_pair_key": f"{red_tactic_condition['family']}-{blue_tactic_condition['family']}",
        "tactic_combo_key": f"{red_tactic_condition['id']}__{blue_tactic_condition['id']}",
        "force_size_key": force_size_key,
        "red_force_size": len(red_template),
        "blue_force_size": len(blue_template),
        "red_attr_bucket": "custom",
        "blue_attr_bucket": "custom",
        "scenario_key": f"{force_size_key}__{red_tactic_condition['id']}__{blue_tactic_condition['id']}",
        "split_group_key": f"{force_size_key}__{red_tactic_condition['id']}__{blue_tactic_condition['id']}",
    }


def _build_combinatorial_specs(args: argparse.Namespace) -> List[Dict[str, object]]:
    tactic_pair_keys = _parse_csv_tokens(args.tactic_pair_keys)
    force_size_keys = _parse_csv_tokens(args.force_size_keys)
    red_attr_keys = _parse_csv_tokens(args.red_attr_buckets)
    blue_attr_keys = _parse_csv_tokens(args.blue_attr_buckets)
    tactic_catalog = _build_tactic_catalog(args)

    specs: List[Dict[str, object]] = []
    for pair_key in tactic_pair_keys:
        if pair_key not in TACTIC_PAIR_TO_FAMILIES:
            raise ValueError(f"Unsupported tactic pair key: {pair_key}")
        red_family, blue_family = TACTIC_PAIR_TO_FAMILIES[pair_key]

        for force_size_key in force_size_keys:
            if force_size_key not in FORCE_SIZE_LIBRARY:
                raise ValueError(f"Unsupported force size key: {force_size_key}")
            force_size = FORCE_SIZE_LIBRARY[force_size_key]

            for red_attr_key in red_attr_keys:
                if red_attr_key not in ATTRIBUTE_BUCKET_LIBRARY:
                    raise ValueError(f"Unsupported red attr bucket: {red_attr_key}")
                for blue_attr_key in blue_attr_keys:
                    if blue_attr_key not in ATTRIBUTE_BUCKET_LIBRARY:
                        raise ValueError(f"Unsupported blue attr bucket: {blue_attr_key}")
                    for red_tactic_condition in tactic_catalog[red_family]:
                        for blue_tactic_condition in tactic_catalog[blue_family]:
                            scenario_key = "__".join(
                                [
                                    pair_key,
                                    force_size_key,
                                    red_attr_key,
                                    blue_attr_key,
                                    str(red_tactic_condition["id"]),
                                    str(blue_tactic_condition["id"]),
                                ]
                            )
                            specs.append(
                                {
                                    "red_tactic_condition": copy.deepcopy(red_tactic_condition),
                                    "blue_tactic_condition": copy.deepcopy(blue_tactic_condition),
                                    "red_template": _build_team_template(int(force_size["red_count"]), red_attr_key),
                                    "blue_template": _build_team_template(int(force_size["blue_count"]), blue_attr_key),
                                    "ranges": {
                                        "red_missile": list(ATTRIBUTE_BUCKET_LIBRARY[red_attr_key]["missile_range"]),
                                        "blue_missile": list(ATTRIBUTE_BUCKET_LIBRARY[blue_attr_key]["missile_range"]),
                                    },
                                    "sampling_meta": {
                                        "sampling_plan": "combinatorial_v1",
                                        "tactic_pair_key": pair_key,
                                        "tactic_combo_key": f"{red_tactic_condition['id']}__{blue_tactic_condition['id']}",
                                        "red_tactic_family": red_family,
                                        "blue_tactic_family": blue_family,
                                        "red_tactic_id": int(red_tactic_condition["params"]["tactic_id"]),
                                        "blue_tactic_id": int(blue_tactic_condition["params"]["tactic_id"]),
                                        "force_size_key": force_size_key,
                                        "red_force_size": int(force_size["red_count"]),
                                        "blue_force_size": int(force_size["blue_count"]),
                                        "red_attr_bucket": red_attr_key,
                                        "blue_attr_bucket": blue_attr_key,
                                        "scenario_key": scenario_key,
                                        "split_group_key": scenario_key,
                                    },
                                }
                            )
    if not specs:
        raise ValueError("Combinatorial sampling produced zero scenario specs.")
    return specs


def _build_combinatorial_schedule(args: argparse.Namespace, rng: random.Random) -> List[Dict[str, object]]:
    base_specs = _build_combinatorial_specs(args)
    specs_by_group: Dict[str, List[Dict[str, object]]] = {}
    for spec in base_specs:
        meta = spec["sampling_meta"]
        group_key = f"{meta['tactic_pair_key']}__{meta['force_size_key']}"
        specs_by_group.setdefault(group_key, []).append(spec)

    active_buckets: Dict[str, List[Dict[str, object]]] = {}
    for group_key, specs in specs_by_group.items():
        active_buckets[group_key] = [copy.deepcopy(spec) for spec in specs]
        rng.shuffle(active_buckets[group_key])

    group_keys = list(specs_by_group.keys())
    scheduled: List[Dict[str, object]] = []
    while len(scheduled) < args.count:
        cycle_group_keys = list(group_keys)
        rng.shuffle(cycle_group_keys)
        for group_key in cycle_group_keys:
            bucket = active_buckets[group_key]
            if not bucket:
                bucket = [copy.deepcopy(spec) for spec in specs_by_group[group_key]]
                rng.shuffle(bucket)
                active_buckets[group_key] = bucket
            scheduled.append(bucket.pop())
            if len(scheduled) >= args.count:
                break
    return scheduled


def _lon_degrees_for_km(km: float, latitude_deg: float) -> float:
    cos_lat = max(0.2, math.cos(math.radians(latitude_deg)))
    return float(km) / (111.32 * cos_lat)


def _lat_degrees_for_km(km: float) -> float:
    return float(km) / 111.0


def _build_position_ranges(cfg: Dict[str, object]) -> Dict[str, List[float]]:
    battlefield = cfg["battlefield"]
    center_lon = (float(battlefield["red_base_lon"]) + float(battlefield["blue_base_lon"])) * 0.5
    center_lat = (float(battlefield["red_base_lat"]) + float(battlefield["blue_base_lat"])) * 0.5

    half_side_lon = _lon_degrees_for_km(float(battlefield["square_side_km"]) * 0.5, center_lat)
    half_side_lat = _lat_degrees_for_km(float(battlefield["square_side_km"]) * 0.5)
    edge_lon_margin = _lon_degrees_for_km(float(battlefield["edge_margin_km"]), center_lat)
    edge_lat_margin = _lat_degrees_for_km(float(battlefield["edge_margin_km"]))
    center_lon_gap = _lon_degrees_for_km(float(battlefield["center_gap_km"]), center_lat)

    left_lon = center_lon - half_side_lon + edge_lon_margin
    right_lon = center_lon + half_side_lon - edge_lon_margin
    bottom_lat = center_lat - half_side_lat + edge_lat_margin
    top_lat = center_lat + half_side_lat - edge_lat_margin

    red_lon_max = center_lon - center_lon_gap
    blue_lon_min = center_lon + center_lon_gap
    if left_lon >= red_lon_max or blue_lon_min >= right_lon or bottom_lat >= top_lat:
        raise ValueError("Invalid battlefield square configuration; ranges collapsed after margins were applied.")

    return {
        "red_lon": [left_lon, red_lon_max],
        "red_lat": [bottom_lat, top_lat],
        "blue_lon": [blue_lon_min, right_lon],
        "blue_lat": [bottom_lat, top_lat],
    }


def _sample_unit(unit_cfg: Dict[str, float], side: str, rng: random.Random, ranges: Dict[str, List[float]]) -> List[float]:
    lon_key = f"{side}_lon"
    lat_key = f"{side}_lat"
    missile_key = f"{side}_missile"

    lon = rng.uniform(float(ranges[lon_key][0]), float(ranges[lon_key][1]))
    lat = rng.uniform(float(ranges[lat_key][0]), float(ranges[lat_key][1]))
    missile = rng.randint(int(ranges[missile_key][0]), int(ranges[missile_key][1]))

    return [
        float(unit_cfg["type_id"]),
        float(unit_cfg["speed"]),
        float(unit_cfg["sensor"]),
        float(missile),
        float(lon),
        float(lat),
        float(unit_cfg.get("alive", 1.0)),
    ]


def _build_task(
    task_idx: int,
    cfg: Dict[str, object],
    rng: random.Random,
    sampled_spec: Optional[Dict[str, object]] = None,
) -> Dict[str, object]:
    ranges = dict(cfg["ranges"])
    ranges.update(_build_position_ranges(cfg))

    if sampled_spec is None:
        red_template = cfg["red_template"]
        blue_template = cfg["blue_template"]
        red_tactic_condition = _resolve_side_tactic_condition(cfg, "red")
        blue_tactic_condition = _resolve_side_tactic_condition(cfg, "blue")
        sampling_meta = _build_simple_sampling_meta(
            red_tactic_condition,
            blue_tactic_condition,
            red_template,
            blue_template,
        )
    else:
        red_template = copy.deepcopy(sampled_spec["red_template"])
        blue_template = copy.deepcopy(sampled_spec["blue_template"])
        red_tactic_condition = copy.deepcopy(sampled_spec["red_tactic_condition"])
        blue_tactic_condition = copy.deepcopy(sampled_spec["blue_tactic_condition"])
        ranges.update(copy.deepcopy(sampled_spec["ranges"]))
        sampling_meta = copy.deepcopy(sampled_spec["sampling_meta"])

    red_features = [_sample_unit(x, "red", rng, ranges) for x in red_template]
    blue_features = [_sample_unit(x, "blue", rng, ranges) for x in blue_template]

    task = {
        "task_id": f"sim_task_{task_idx:05d}",
        "red_tactic_condition": red_tactic_condition,
        "blue_tactic_condition": blue_tactic_condition,
        "initial_state": {
            "red_features": red_features,
            "blue_features": blue_features,
        },
        "sampling_meta": sampling_meta,
    }
    if red_tactic_condition == blue_tactic_condition:
        task["tactic_id"] = int(red_tactic_condition["params"]["tactic_id"])
    return task


def _build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate black-box simulation task packets (7D initial features).")
    parser.add_argument("--out-path", type=Path, default=Path("agent_mvp/data_real/raw/simulation_tasks.jsonl"))
    parser.add_argument("--count", type=int, default=int(USER_CONFIG_ZONE["task_count"]))
    parser.add_argument("--seed", type=int, default=int(USER_CONFIG_ZONE["seed"]))

    # Single-config Phase 2.5 overrides.
    parser.add_argument("--red-source", type=str, default=None)
    parser.add_argument("--red-family", type=str, default=None)
    parser.add_argument("--red-id", type=str, default=None)
    parser.add_argument("--red-tactic-id", type=int, default=None)
    parser.add_argument("--blue-source", type=str, default=None)
    parser.add_argument("--blue-family", type=str, default=None)
    parser.add_argument("--blue-id", type=str, default=None)
    parser.add_argument("--blue-tactic-id", type=int, default=None)

    # Phase 3 combinatorial sampling.
    parser.add_argument("--sampling-plan", choices=("simple", "combinatorial"), default="simple")
    parser.add_argument("--catalog-source", type=str, default="task_generator")
    parser.add_argument("--rule-tactic-ids", type=str, default="1,3")
    parser.add_argument("--rl-tactic-ids", type=str, default="2,4")
    parser.add_argument("--tactic-pair-keys", type=str, default=",".join(DEFAULT_TACTIC_PAIR_KEYS))
    parser.add_argument("--force-size-keys", type=str, default=",".join(DEFAULT_FORCE_SIZE_KEYS))
    parser.add_argument("--red-attr-buckets", type=str, default=",".join(DEFAULT_ATTR_BUCKET_KEYS))
    parser.add_argument("--blue-attr-buckets", type=str, default=",".join(DEFAULT_ATTR_BUCKET_KEYS))
    return parser


def main() -> None:
    parser = _build_arg_parser()
    args = parser.parse_args()

    out_path = args.out_path
    out_path.parent.mkdir(parents=True, exist_ok=True)

    cfg = _apply_cli_tactic_overrides(USER_CONFIG_ZONE, args)
    rng = random.Random(args.seed)

    if args.sampling_plan == "combinatorial":
        schedule = _build_combinatorial_schedule(args, rng)
    else:
        schedule = [None] * args.count

    with out_path.open("w", encoding="utf-8") as f:
        for index, sampled_spec in enumerate(schedule, start=1):
            task = _build_task(index, cfg, rng, sampled_spec=sampled_spec)
            f.write(json.dumps(task, ensure_ascii=True) + "\n")

    print(f"Generated {args.count} tasks -> {out_path}")


if __name__ == "__main__":
    main()
