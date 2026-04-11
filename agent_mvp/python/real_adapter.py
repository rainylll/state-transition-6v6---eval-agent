from typing import Dict, List, Tuple


RAW_FEATURE_ORDER = ("type_id", "speed", "sensor", "missile", "lon", "lat", "alive")
UNIT_VECTOR_ORDER = ("speed", "sensor", "missile", "alive", "relative_distance")


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def _safe_float(v, default: float = 0.0) -> float:
    try:
        return float(v)
    except (TypeError, ValueError):
        return default


def _norm(v: float, lo: float, hi: float) -> float:
    if hi <= lo:
        return 0.0
    return _clamp01((v - lo) / (hi - lo))


def _map_entity(raw: Dict) -> Dict[str, float]:
    # New input schema: [type_id, speed, sensor, missile, lon, lat, alive].
    raw_features = raw.get("features")
    if isinstance(raw_features, list) and len(raw_features) >= 7:
        type_id = int(round(_safe_float(raw_features[0], 0.0)))
        speed = _safe_float(raw_features[1], 180.0)
        sensor = _safe_float(raw_features[2], 80.0)
        missile = _safe_float(raw_features[3], 2.0)
        lon = _safe_float(raw_features[4], 0.0)
        lat = _safe_float(raw_features[5], 0.0)
        alive = _safe_float(raw_features[6], 1.0)
    elif isinstance(raw_features, list) and len(raw_features) >= 6:
        # Backward compatibility for old raw schema without type_id.
        type_id = int(round(_safe_float(raw.get("type_id"), 0.0)))
        speed = _safe_float(raw_features[0], 180.0)
        sensor = _safe_float(raw_features[1], 80.0)
        missile = _safe_float(raw_features[2], 2.0)
        lon = _safe_float(raw_features[3], 0.0)
        lat = _safe_float(raw_features[4], 0.0)
        alive = _safe_float(raw_features[5], 1.0)
    else:
        type_id = int(round(_safe_float(raw.get("type_id"), 0.0)))
        speed = _safe_float(raw.get("speed_mps"), _safe_float(raw.get("speed"), _safe_float(raw.get("speed_knots"), 18.0) * 0.514444))
        sensor = _safe_float(raw.get("radar_range_km"), _safe_float(raw.get("sensor"), _safe_float(raw.get("sensor_range"), 80.0)))
        missile = _safe_float(raw.get("missile_count"), _safe_float(raw.get("a2a_missiles"), _safe_float(raw.get("sam_count"), 2.0)))
        lon = _safe_float(raw.get("lon"), _safe_float(raw.get("longitude"), 0.0))
        lat = _safe_float(raw.get("lat"), _safe_float(raw.get("latitude"), 0.0))
        alive = _safe_float(raw.get("alive"), _safe_float(raw.get("is_alive"), 1.0 if _safe_float(raw.get("integrity"), 1.0) > 0.0 else 0.0))

    return {
        "id": raw.get("id"),
        "type_id": int(type_id),
        "raw_features": [float(type_id), speed, sensor, missile, lon, lat, _clamp01(alive)],
        "speed": speed,
        "sensor": sensor,
        "missile": missile,
        "alive": _clamp01(alive),
        "meta": {"lon": lon, "lat": lat},
    }


def _centroid(units: List[Dict[str, float]]) -> Tuple[float, float]:
    if not units:
        return 0.0, 0.0
    lon = sum(_safe_float(u.get("meta", {}).get("lon"), 0.0) for u in units) / len(units)
    lat = sum(_safe_float(u.get("meta", {}).get("lat"), 0.0) for u in units) / len(units)
    return lon, lat


def _relative_distance_feature(red_units: List[Dict[str, float]], blue_units: List[Dict[str, float]]) -> float:
    r_lon, r_lat = _centroid(red_units)
    b_lon, b_lat = _centroid(blue_units)
    dlon_km = (r_lon - b_lon) * 111.0
    dlat_km = (r_lat - b_lat) * 111.0
    dist_km = (dlon_km * dlon_km + dlat_km * dlat_km) ** 0.5
    return _clamp01(_norm(dist_km, 0.0, 400.0))


def _attach_relative_distance(units: List[Dict[str, float]], rel_dist: float) -> List[Dict[str, float]]:
    out: List[Dict[str, float]] = []
    for u in units:
        speed = _safe_float(u.get("speed"), 0.0)
        sensor = _safe_float(u.get("sensor"), 0.0)
        missile = _safe_float(u.get("missile"), 0.0)
        alive = _clamp01(_safe_float(u.get("alive"), 0.0))
        # Model features intentionally drop absolute coordinates.
        features = [speed, sensor, missile, alive, rel_dist]
        v = dict(u)
        v["relative_distance"] = rel_dist
        v["features"] = features
        out.append(v)
    return out


def _ensure_non_empty(units: List[Dict[str, float]]) -> List[Dict[str, float]]:
    if units:
        return units
    # Keep one neutral placeholder to avoid all-masked attention edge cases.
    return [
        {
            "id": None,
            "raw_features": [0.0 for _ in RAW_FEATURE_ORDER],
            "type_id": 0,
            "speed": 0.0,
            "sensor": 0.0,
            "missile": 0.0,
            "alive": 0.0,
            "meta": {"lon": 0.0, "lat": 0.0},
        }
    ]


def _map_side(side_payload: Dict) -> List[Dict[str, float]]:
    if isinstance(side_payload, list):
        return _ensure_non_empty([_map_entity(entity) for entity in side_payload])

    out: List[Dict[str, float]] = []
    for entity in side_payload.get("aircraft", []):
        out.append(_map_entity(entity))
    for entity in side_payload.get("ships", []):
        out.append(_map_entity(entity))
    for entity in side_payload.get("units", []):
        out.append(_map_entity(entity))
    return _ensure_non_empty(out)


def _map_tactic(cmd: Dict) -> List[float]:
    launch_timing = _clamp01(_norm(_safe_float(cmd.get("launch_delay_s"), 60.0), 0.0, 600.0))
    formation = _clamp01(_norm(_safe_float(cmd.get("formation_compactness"), 0.5), 0.0, 1.0))
    target_assignment = _clamp01(_norm(_safe_float(cmd.get("target_focus_ratio"), 0.5), 0.0, 1.0))
    attack_axis = _clamp01(_norm(abs(_safe_float(cmd.get("attack_bearing_deg"), 45.0)), 0.0, 180.0))
    support_style = _clamp01(_norm(_safe_float(cmd.get("support_level"), 0.5), 0.0, 1.0))
    return [launch_timing, formation, target_assignment, attack_axis, support_style]


def adapt_real_payload_to_sample(payload: Dict) -> Dict:
    """Convert one raw engine-like JSON payload to current model sample schema."""
    state = payload.get("initial_state", {}) if isinstance(payload.get("initial_state"), dict) else {}
    red_units = _map_side(payload.get("red", state.get("red", {})))
    blue_units = _map_side(payload.get("blue", state.get("blue", {})))
    rel_dist = _relative_distance_feature(red_units, blue_units)
    red_units = _attach_relative_distance(red_units, rel_dist)
    blue_units = _attach_relative_distance(blue_units, rel_dist)
    tactic = _map_tactic(payload.get("tactic_cmd", {}))

    return {
        "red_units": red_units,
        "blue_units": blue_units,
        "global": {"relative_distance": rel_dist},
        "tactic": tactic,
    }


def looks_like_real_payload(payload: Dict) -> bool:
    if "initial_state" in payload and isinstance(payload["initial_state"], dict):
        return True
    return "red" in payload and "blue" in payload and (
        "aircraft" in payload.get("red", {})
        or "ships" in payload.get("red", {})
        or "aircraft" in payload.get("blue", {})
        or "ships" in payload.get("blue", {})
    )


def infer_counts(sample: Dict) -> Tuple[int, int]:
    return len(sample.get("red_units", [])), len(sample.get("blue_units", []))

