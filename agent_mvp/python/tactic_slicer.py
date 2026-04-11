from dataclasses import asdict, dataclass
from itertools import combinations
from typing import Any, Dict, List, Optional, Sequence, Tuple

import math

try:
    import torch
except Exception:  # pragma: no cover - optional runtime dependency
    torch = None


@dataclass
class SlicedTacticEval:
    template: str
    slice_count: int
    per_slice_red_win_prob: List[float]
    mean_red_win_prob: float
    assignments: List[Dict[str, Any]]


def _feature_vector(unit: Dict[str, Any]) -> List[float]:
    if "raw_features" in unit:
        raw = [float(x) for x in unit["raw_features"]]
        if len(raw) >= 7:
            # [type_id, speed, sensor, missile, lon, lat, alive] -> canonical proxy.
            return [raw[1], raw[2], raw[3], raw[4], raw[5], raw[6]]
        return raw
    if "features" in unit:
        feat = [float(x) for x in unit["features"]]
        # Minimal model feature payload may already drop lon/lat.
        if len(feat) == 5:
            return [feat[0], feat[1], feat[2], 0.0, 0.0, feat[3]]
        return feat
    return [
        float(unit.get("speed", 0.0)),
        float(unit.get("sensor_range", unit.get("sensor", 0.0))),
        float(unit.get("missile", unit.get("weapon", 0.0))),
        float(unit.get("lon", 0.0)),
        float(unit.get("lat", 0.0)),
        float(unit.get("alive", 1.0)),
    ]


def _xy(unit: Dict[str, Any]) -> Optional[Tuple[float, float]]:
    if "raw_features" in unit and isinstance(unit["raw_features"], list) and len(unit["raw_features"]) >= 5:
        if len(unit["raw_features"]) >= 7:
            return float(unit["raw_features"][4]), float(unit["raw_features"][5])
        return float(unit["raw_features"][3]), float(unit["raw_features"][4])
    # Prefer metadata positions when present.
    if "meta" in unit and isinstance(unit["meta"], dict):
        meta = unit["meta"]
        if "lon" in meta and "lat" in meta and meta["lon"] is not None and meta["lat"] is not None:
            return float(meta["lon"]), float(meta["lat"])
        if "x" in meta and "y" in meta and meta["x"] is not None and meta["y"] is not None:
            return float(meta["x"]), float(meta["y"])
    if "lon" in unit and "lat" in unit and unit["lon"] is not None and unit["lat"] is not None:
        return float(unit["lon"]), float(unit["lat"])
    if "x" in unit and "y" in unit and unit["x"] is not None and unit["y"] is not None:
        return float(unit["x"]), float(unit["y"])
    return None


def _distance_proxy(red_unit: Dict[str, Any], blue_unit: Dict[str, Any]) -> float:
    red_xy = _xy(red_unit)
    blue_xy = _xy(blue_unit)
    if red_xy is None or blue_xy is None:
        raise ValueError("Slicer requires absolute coordinates (lon/lat or x/y) for distance-aware assignment.")
    dx = red_xy[0] - blue_xy[0]
    dy = red_xy[1] - blue_xy[1]
    return math.sqrt(dx * dx + dy * dy)


def _threat_score(red_unit: Dict[str, Any], blue_unit: Dict[str, Any]) -> float:
    rf = _feature_vector(red_unit)
    bf = _feature_vector(blue_unit)
    red_offense = 0.55 * rf[2] + 0.25 * rf[1] + 0.15 * min(max(rf[0] / 350.0, 0.0), 1.0) + 0.05 * rf[5]
    blue_resist = 0.50 * bf[2] + 0.20 * bf[1] + 0.20 * min(max(bf[0] / 350.0, 0.0), 1.0) + 0.10 * bf[5]
    return red_offense - blue_resist


def _pair_score(red_pair: Sequence[Dict[str, Any]], blue_unit: Dict[str, Any], strategy: str) -> float:
    dist = sum(_distance_proxy(r, blue_unit) for r in red_pair) / max(len(red_pair), 1)
    threat = sum(_threat_score(r, blue_unit) for r in red_pair)
    if strategy == "distance":
        return -dist
    if strategy == "threat":
        return threat
    # distance_threat
    return threat - 0.15 * dist


def _solve_4v2_assignment(
    red_units: List[Dict[str, Any]],
    blue_units: List[Dict[str, Any]],
    strategy: str,
) -> List[Tuple[List[int], int, float]]:
    best = None
    red_idx = list(range(4))
    for pair in combinations(red_idx, 2):
        left = [i for i in red_idx if i not in pair]
        pair_a = list(pair)
        pair_b = left

        score_0 = _pair_score([red_units[i] for i in pair_a], blue_units[0], strategy)
        score_1 = _pair_score([red_units[i] for i in pair_b], blue_units[1], strategy)
        total_a = score_0 + score_1

        score_0_swap = _pair_score([red_units[i] for i in pair_a], blue_units[1], strategy)
        score_1_swap = _pair_score([red_units[i] for i in pair_b], blue_units[0], strategy)
        total_b = score_0_swap + score_1_swap

        candidate = (
            [(pair_a, 0, score_0), (pair_b, 1, score_1)]
            if total_a >= total_b
            else [(pair_a, 1, score_0_swap), (pair_b, 0, score_1_swap)]
        )
        candidate_total = total_a if total_a >= total_b else total_b

        if best is None or candidate_total > best[0]:
            best = (candidate_total, candidate)

    if best is None:
        raise ValueError("Failed to compute 4v2 assignment.")
    return best[1]


def slice_4v2_to_2v1(macro_sample: Dict[str, Any], strategy: str = "distance_threat") -> List[Dict[str, Any]]:
    """Split one 4v2 sample into two local 2v1 encounters using a strategy-aware assignment."""
    red_units = list(macro_sample.get("red_units", []))
    blue_units = list(macro_sample.get("blue_units", []))
    tactic = list(macro_sample.get("tactic", [0.0, 0.0, 0.0, 0.0, 0.0]))

    if len(red_units) < 4 or len(blue_units) < 2:
        raise ValueError("2v1 slicer expects at least 4 red units and 2 blue units in macro sample.")
    if strategy not in {"distance_threat", "distance", "threat"}:
        raise ValueError("strategy must be one of: distance_threat, distance, threat")

    assignments = _solve_4v2_assignment(red_units[:4], blue_units[:2], strategy)
    slices: List[Dict[str, Any]] = []
    for red_idx_pair, blue_idx, pair_score in assignments:
        slices.append(
            {
                "red_units": [red_units[i] for i in red_idx_pair],
                "blue_units": [blue_units[blue_idx]],
                "tactic": tactic,
                "slice_meta": {
                    "strategy": strategy,
                    "red_indices": red_idx_pair,
                    "blue_index": blue_idx,
                    "pair_score": float(pair_score),
                    "red_ids": [red_units[i].get("id") for i in red_idx_pair],
                    "blue_id": blue_units[blue_idx].get("id"),
                },
            }
        )
    return slices


def evaluate_2v1_slices_heuristic(
    macro_sample: Dict[str, Any],
    tactic_template: str = "2v1",
    strategy: str = "distance_threat",
) -> Dict[str, Any]:
    if tactic_template != "2v1":
        raise ValueError("Current skeleton supports tactic_template='2v1' only.")

    local_slices = slice_4v2_to_2v1(macro_sample, strategy=strategy)
    probs: List[float] = []
    assignments: List[Dict[str, Any]] = []

    for local in local_slices:
        red_pair = local["red_units"]
        blue_u = local["blue_units"][0]
        score = _pair_score(red_pair, blue_u, strategy)
        # Smooth mapping to [0,1] probability without requiring a model.
        prob = 1.0 / (1.0 + math.exp(-score))
        probs.append(float(prob))
        assignments.append(local.get("slice_meta", {}))

    mean_prob = float(sum(probs) / max(len(probs), 1))
    return asdict(
        SlicedTacticEval(
            template=tactic_template,
            slice_count=len(local_slices),
            per_slice_red_win_prob=probs,
            mean_red_win_prob=mean_prob,
            assignments=assignments,
        )
    )


def evaluate_2v1_slices_with_model(
    model: Any,
    macro_sample: Dict[str, Any],
    tactic_template: str = "2v1",
    strategy: str = "distance_threat",
) -> Dict[str, Any]:
    """Slice macro forces into local encounters, run model.forward() for each, and average red win probability."""
    if tactic_template != "2v1":
        raise ValueError("Current skeleton supports tactic_template='2v1' only.")
    if torch is None:
        raise RuntimeError("PyTorch is required for model-based slicer evaluation.")

    from dataset import collate_infer_batch, encode_units

    local_slices = slice_4v2_to_2v1(macro_sample, strategy=strategy)
    infer_items = []
    for local in local_slices:
        infer_items.append(
            {
                "red_units": encode_units(local["red_units"]),
                "blue_units": encode_units(local["blue_units"]),
                "tactic": torch.tensor(local["tactic"], dtype=torch.float32),
            }
        )

    batch = collate_infer_batch(infer_items)

    # Infer on the same device where model parameters live.
    device = next(model.parameters()).device
    batch = {k: v.to(device) for k, v in batch.items()}

    model.eval()
    with torch.no_grad():
        out = model(batch)
        probs = out["win_rate"].detach().cpu().tolist()

    mean_prob = float(sum(float(p) for p in probs) / max(len(probs), 1))
    return asdict(
        SlicedTacticEval(
            template=tactic_template,
            slice_count=len(local_slices),
            per_slice_red_win_prob=[float(p) for p in probs],
            mean_red_win_prob=mean_prob,
            assignments=[x.get("slice_meta", {}) for x in local_slices],
        )
    )

