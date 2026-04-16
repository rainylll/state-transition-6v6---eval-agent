import hashlib
import json
from typing import Any, Dict, List, Tuple

# Keep a stable, shared subset so grouping is reproducible across training/eval/report scripts.
UNIT_SIGNATURE_FIELDS = (
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


def _to_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _to_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _canonical_unit(side: str, unit: Dict[str, Any]) -> Dict[str, Any]:
    return {
        "side": str(side),
        "unit_id": str(unit.get("unit_id", "")),
        "type_id": _to_float(unit.get("type_id", 0.0)),
        "alive": _to_int(unit.get("alive", 0), 0),
        "missile_count": _to_int(unit.get("missile_count", 0), 0),
        "lon": round(_to_float(unit.get("lon", 0.0)), 6),
        "lat": round(_to_float(unit.get("lat", 0.0)), 6),
        "alt_m": round(_to_float(unit.get("alt_m", 0.0)), 3),
        "speed_mps": round(_to_float(unit.get("speed_mps", 0.0)), 3),
        "heading_deg": round(_to_float(unit.get("heading_deg", 0.0)) % 360.0, 3),
    }


def canonical_state_payload(state: Dict[str, Any]) -> Dict[str, Any]:
    units: List[Dict[str, Any]] = []
    for side in ("red", "blue"):
        side_units = state.get(f"{side}_units", [])
        if not isinstance(side_units, list):
            continue
        for unit in side_units:
            if not isinstance(unit, dict):
                continue
            units.append(_canonical_unit(side, unit))

    units.sort(
        key=lambda item: (
            item["side"],
            item["unit_id"],
            item["type_id"],
            item["lon"],
            item["lat"],
            item["alt_m"],
            item["speed_mps"],
            item["heading_deg"],
            item["missile_count"],
            item["alive"],
        )
    )
    return {"units": units}


def canonical_state_signature(state: Dict[str, Any]) -> str:
    payload = canonical_state_payload(state)
    raw = json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True)
    return hashlib.sha1(raw.encode("utf-8")).hexdigest()


def canonical_state_rows(state: Dict[str, Any]) -> List[Tuple[str, str, float, float, float, float, float, int, int]]:
    payload = canonical_state_payload(state)
    rows: List[Tuple[str, str, float, float, float, float, float, int, int]] = []
    for item in payload["units"]:
        rows.append(
            (
                item["side"],
                item["unit_id"],
                float(item["lon"]),
                float(item["lat"]),
                float(item["alt_m"]),
                float(item["speed_mps"]),
                float(item["heading_deg"]),
                int(item["missile_count"]),
                int(item["alive"]),
            )
        )
    return rows


def canonical_signature_from_record(record: Dict[str, Any]) -> str:
    return canonical_state_signature(record.get("state_t", {}))
