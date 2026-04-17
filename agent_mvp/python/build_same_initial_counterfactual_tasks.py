import argparse
import copy
import json
import math
import random
from pathlib import Path
from typing import Dict, List, Sequence, Tuple


def _parse_csv_ints(raw: str) -> List[int]:
    values: List[int] = []
    for token in raw.split(","):
        item = token.strip()
        if not item:
            continue
        values.append(int(item))
    if not values:
        raise ValueError("Expected at least one tactic id.")
    return values


def _lon_degrees_for_km(km: float, latitude_deg: float) -> float:
    cos_lat = max(0.2, math.cos(math.radians(latitude_deg)))
    return float(km) / (111.32 * cos_lat)


def _lat_degrees_for_km(km: float) -> float:
    return float(km) / 111.0


def _build_position_ranges() -> Dict[str, List[float]]:
    red_base_lon = 116.2
    red_base_lat = 21.577106
    blue_base_lon = 119.8
    blue_base_lat = 21.577106
    square_side_km = 600.0
    edge_margin_km = 60.0
    center_gap_km = 30.0

    center_lon = (red_base_lon + blue_base_lon) * 0.5
    center_lat = (red_base_lat + blue_base_lat) * 0.5

    half_side_lon = _lon_degrees_for_km(square_side_km * 0.5, center_lat)
    half_side_lat = _lat_degrees_for_km(square_side_km * 0.5)
    edge_lon_margin = _lon_degrees_for_km(edge_margin_km, center_lat)
    edge_lat_margin = _lat_degrees_for_km(edge_margin_km)
    center_lon_gap = _lon_degrees_for_km(center_gap_km, center_lat)

    left_lon = center_lon - half_side_lon + edge_lon_margin
    right_lon = center_lon + half_side_lon - edge_lon_margin
    bottom_lat = center_lat - half_side_lat + edge_lat_margin
    top_lat = center_lat + half_side_lat - edge_lat_margin

    red_lon_max = center_lon - center_lon_gap
    blue_lon_min = center_lon + center_lon_gap

    return {
        "red_lon": [left_lon, red_lon_max],
        "red_lat": [bottom_lat, top_lat],
        "blue_lon": [blue_lon_min, right_lon],
        "blue_lat": [bottom_lat, top_lat],
    }


def _sample_unit_feature(
    side: str,
    rng: random.Random,
    ranges: Dict[str, List[float]],
    missile_range: Tuple[int, int],
    speed: float,
    sensor: float,
) -> List[float]:
    lon = rng.uniform(ranges[f"{side}_lon"][0], ranges[f"{side}_lon"][1])
    lat = rng.uniform(ranges[f"{side}_lat"][0], ranges[f"{side}_lat"][1])
    missile = rng.randint(int(missile_range[0]), int(missile_range[1]))
    return [0.0, float(speed), float(sensor), float(missile), float(lon), float(lat), 1.0]


def _sample_initial_state(
    red_count: int,
    blue_count: int,
    rng: random.Random,
) -> Dict[str, List[List[float]]]:
    ranges = _build_position_ranges()
    red_features = [
        _sample_unit_feature("red", rng, ranges, missile_range=(2, 5), speed=300.0, sensor=120.0)
        for _ in range(red_count)
    ]
    blue_features = [
        _sample_unit_feature("blue", rng, ranges, missile_range=(2, 5), speed=300.0, sensor=120.0)
        for _ in range(blue_count)
    ]
    return {
        "red_features": red_features,
        "blue_features": blue_features,
    }


def _tactic_condition(tactic_id: int, side: str) -> Dict[str, object]:
    return {
        "source": "task_generator",
        "family": "rule",
        "id": f"tactic_{int(tactic_id)}",
        "side": side,
        "params": {"tactic_id": int(tactic_id)},
    }


def _build_variant_combos(red_ids: Sequence[int], blue_ids: Sequence[int], max_variants: int) -> List[Tuple[int, int]]:
    if len(red_ids) < 2 or len(blue_ids) < 2:
        raise ValueError("Need at least two red and two blue tactic ids for counterfactual variants.")

    r0, r1 = red_ids[0], red_ids[1]
    b0, b1 = blue_ids[0], blue_ids[1]
    combos: List[Tuple[int, int]] = [
        (r0, b0),
        (r0, b1),  # red fixed, blue changes
        (r1, b0),  # blue fixed, red changes
        (r1, b1),  # both change
    ]

    for rid in red_ids[2:]:
        combos.append((rid, b1))
    for bid in blue_ids[2:]:
        combos.append((r1, bid))

    # Fill with cartesian if requested more variants.
    if len(combos) < max_variants:
        for rid in red_ids:
            for bid in blue_ids:
                pair = (rid, bid)
                if pair not in combos:
                    combos.append(pair)
                if len(combos) >= max_variants:
                    break
            if len(combos) >= max_variants:
                break

    return combos[:max_variants]


def main() -> None:
    parser = argparse.ArgumentParser(description="Build same-initial/different-tactic task pack.")
    parser.add_argument("--out-path", type=Path, default=Path("agent_mvp/data_real/raw/simulation_tasks.jsonl"))
    parser.add_argument("--seed", type=int, default=20260415)
    parser.add_argument("--num-groups", type=int, default=24)
    parser.add_argument("--heldout-groups", type=int, default=10)
    parser.add_argument("--variants-per-group", type=int, default=5)
    parser.add_argument("--red-count", type=int, default=2)
    parser.add_argument("--blue-count", type=int, default=1)
    parser.add_argument("--red-tactic-ids", type=str, default="1,2,3")
    parser.add_argument("--blue-tactic-ids", type=str, default="1,2,3")
    args = parser.parse_args()

    if args.num_groups <= 0:
        raise ValueError("num-groups must be positive")
    if args.variants_per_group < 4:
        raise ValueError("variants-per-group must be at least 4 to cover required counterfactual patterns")

    red_ids = _parse_csv_ints(args.red_tactic_ids)
    blue_ids = _parse_csv_ints(args.blue_tactic_ids)
    variant_combos = _build_variant_combos(red_ids, blue_ids, max_variants=args.variants_per_group)

    rng = random.Random(args.seed)
    group_indices = list(range(args.num_groups))
    rng.shuffle(group_indices)
    heldout_set = set(group_indices[: max(0, min(args.heldout_groups, args.num_groups))])

    tasks: List[Dict[str, object]] = []
    task_index = 1

    for group_index in range(args.num_groups):
        group_id = f"si_cf_group_{group_index:04d}"
        pack_split = "heldout" if group_index in heldout_set else "train_visible"
        initial_state = _sample_initial_state(args.red_count, args.blue_count, rng)

        for red_tid, blue_tid in variant_combos:
            red_cond = _tactic_condition(red_tid, side="red")
            blue_cond = _tactic_condition(blue_tid, side="blue")
            tactic_combo_key = f"{red_cond['id']}__{blue_cond['id']}"
            sampling_meta = {
                "sampling_plan": "same_initial_counterfactual_v1",
                "same_initial_group_id": group_id,
                "counterfactual_pack_split": pack_split,
                "tactic_pair_key": "rule-rule",
                "tactic_combo_key": tactic_combo_key,
                "red_tactic_family": "rule",
                "blue_tactic_family": "rule",
                "red_tactic_id": int(red_tid),
                "blue_tactic_id": int(blue_tid),
                "force_size_key": f"{args.red_count}v{args.blue_count}",
                "red_force_size": int(args.red_count),
                "blue_force_size": int(args.blue_count),
                "red_attr_bucket": "same_initial_balanced",
                "blue_attr_bucket": "same_initial_balanced",
                "scenario_key": f"{group_id}__{tactic_combo_key}",
                "split_group_key": f"{group_id}",
            }

            tasks.append(
                {
                    "task_id": f"sim_task_{task_index:05d}",
                    "red_tactic_condition": red_cond,
                    "blue_tactic_condition": blue_cond,
                    "initial_state": copy.deepcopy(initial_state),
                    "sampling_meta": sampling_meta,
                }
            )
            task_index += 1

    args.out_path.parent.mkdir(parents=True, exist_ok=True)
    with args.out_path.open("w", encoding="utf-8") as f:
        for task in tasks:
            f.write(json.dumps(task, ensure_ascii=True) + "\n")

    print(
        "Same-initial counterfactual tasks generated | "
        f"groups={args.num_groups} | variants_per_group={len(variant_combos)} | "
        f"total_tasks={len(tasks)} | heldout_groups={len(heldout_set)}"
    )


if __name__ == "__main__":
    main()
