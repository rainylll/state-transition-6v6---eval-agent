import argparse
import json
import math
import random
from pathlib import Path
from typing import Dict, List


# ==========================================================
# USER_CONFIG_ZONE (edit this section only for quick custom batches)
# - type_id: combat unit type code, independent of side
#   one code represents one unit kind; the same unit kind on red/blue
#   should use the same type_id
# - speed/sensor/alive are fixed per template entry
# - lon/lat/missile are randomized by generator logic below
# - 【修改兵力规模】：直接增删 red_template 和 blue_template 列表中的字典个数即可。
#   例如 red_template 中有 3 个字典，就代表生成 3 个红方单位。
# ==========================================================
USER_CONFIG_ZONE = {
    "task_count": 1000,
    "seed": 2026,
    "tactic_id": 1,
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


def _build_task(task_idx: int, cfg: Dict[str, object], rng: random.Random) -> Dict[str, object]:
    ranges = dict(cfg["ranges"])
    ranges.update(_build_position_ranges(cfg))
    red_template = cfg["red_template"]
    blue_template = cfg["blue_template"]

    red_features = [_sample_unit(x, "red", rng, ranges) for x in red_template]
    blue_features = [_sample_unit(x, "blue", rng, ranges) for x in blue_template]

    return {
        "task_id": f"sim_task_{task_idx:05d}",
        "tactic_id": int(cfg["tactic_id"]),
        "initial_state": {
            "red_features": red_features,
            "blue_features": blue_features,
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate black-box simulation task packets (7D initial features).")
    parser.add_argument("--out-path", type=Path, default=Path("agent_mvp/data_real/raw/simulation_tasks.jsonl"))
    parser.add_argument("--count", type=int, default=int(USER_CONFIG_ZONE["task_count"]))
    parser.add_argument("--seed", type=int, default=int(USER_CONFIG_ZONE["seed"]))
    args = parser.parse_args()

    out_path = args.out_path
    out_path.parent.mkdir(parents=True, exist_ok=True)

    cfg = dict(USER_CONFIG_ZONE)
    rng = random.Random(args.seed)

    with out_path.open("w", encoding="utf-8") as f:
        for i in range(1, args.count + 1):
            task = _build_task(i, cfg, rng)
            f.write(json.dumps(task, ensure_ascii=True) + "\n")

    print(f"Generated {args.count} tasks -> {out_path}")


if __name__ == "__main__":
    main()

