import argparse
import json
import math
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional

PRIORITY_ORDER = {"P0": 0, "P1": 1, "P2": 2}


def _is_number(v: Any) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(float(v))


def _episode_ref(ep: Dict[str, Any], line_no: int) -> str:
    episode_id = ep.get("episode_id")
    if isinstance(episode_id, str) and episode_id.strip():
        return episode_id
    return f"<line:{line_no}>"


def _get_path(payload: Dict[str, Any], path: str) -> Any:
    cur: Any = payload
    for token in path.split("."):
        if not isinstance(cur, dict) or token not in cur:
            raise KeyError(path)
        cur = cur[token]
    return cur


def _iter_json_records(input_path: Path) -> Iterable[Dict[str, Any]]:
    if input_path.suffix.lower() == ".jsonl":
        with input_path.open("r", encoding="utf-8") as f:
            for line_no, line in enumerate(f, start=1):
                raw = line.strip()
                if not raw:
                    continue
                try:
                    payload = json.loads(raw)
                except Exception as exc:
                    raise ValueError(f"Invalid JSON at line {line_no}: {exc}") from exc
                if not isinstance(payload, dict):
                    raise ValueError(f"Line {line_no} is not a JSON object.")
                payload["__line_no__"] = line_no
                yield payload
        return

    with input_path.open("r", encoding="utf-8") as f:
        payload = json.load(f)

    episodes: Optional[List[Dict[str, Any]]] = None
    if isinstance(payload, list):
        episodes = payload
    elif isinstance(payload, dict) and isinstance(payload.get("episodes"), list):
        episodes = payload["episodes"]

    if episodes is None:
        raise ValueError("JSON input must be a list or an object with key 'episodes'.")

    for idx, episode in enumerate(episodes, start=1):
        if not isinstance(episode, dict):
            raise ValueError(f"Episode index {idx} is not a JSON object.")
        episode["__line_no__"] = idx
        yield episode


def _append_error(
    errors: List[Dict[str, Any]],
    priority: str,
    line_no: int,
    episode_id: str,
    field: str,
    msg: str,
    value: Any = None,
) -> None:
    errors.append(
        {
            "priority": priority,
            "line": line_no,
            "episode_id": episode_id,
            "field": field,
            "message": msg,
            "value": value,
        }
    )


def _validate_unit_fields(
    errors: List[Dict[str, Any]],
    episode_id: str,
    line_no: int,
    side_name: str,
    unit_list_name: str,
    units: List[Any],
) -> None:
    aircraft_required = [
        "speed_mps",
        "radar_range_km",
        "a2a_missiles",
        "gun_rounds",
        "jammer_level",
        "chaff_count",
        "fuel_ratio",
        "integrity",
        "awacs_link_quality",
    ]
    ship_required = [
        "speed_knots",
        "radar_range_km",
        "sam_count",
        "ashm_count",
        "ciws_level",
        "ecm_level",
        "displacement_ton",
        "integrity",
        "ew_support",
    ]

    p1_required = aircraft_required if unit_list_name == "aircraft" else ship_required

    for i, unit in enumerate(units):
        base = f"initial_state.{side_name}.{unit_list_name}[{i}]"
        if not isinstance(unit, dict):
            _append_error(errors, "P0", line_no, episode_id, base, "unit must be object", unit)
            continue

        for field in p1_required:
            if field not in unit:
                _append_error(errors, "P1", line_no, episode_id, f"{base}.{field}", "missing recommended field")

        # Numeric sanity checks for fields if they exist.
        for f_name in ["speed_mps", "speed_knots", "radar_range_km", "a2a_missiles", "missile_count", "sam_count", "ashm_count", "gun_rounds", "chaff_count", "displacement_ton"]:
            if f_name in unit and not _is_number(unit[f_name]):
                _append_error(errors, "P1", line_no, episode_id, f"{base}.{f_name}", "must be finite number", unit[f_name])

        for f_name in ["jammer_level", "ciws_level", "ecm_level", "fuel_ratio", "integrity", "awacs_link_quality", "ew_support", "hp", "ecm"]:
            if f_name in unit:
                v = unit[f_name]
                if not _is_number(v):
                    _append_error(errors, "P1", line_no, episode_id, f"{base}.{f_name}", "must be finite number", v)
                elif not (0.0 <= float(v) <= 1.0):
                    _append_error(errors, "P1", line_no, episode_id, f"{base}.{f_name}", "must be in [0,1]", v)

        if "type_id" in unit:
            v = unit["type_id"]
            if not isinstance(v, int) or isinstance(v, bool) or v not in (0, 1):
                _append_error(errors, "P1", line_no, episode_id, f"{base}.type_id", "must be int in {0,1}", v)


def _validate_episode(ep: Dict[str, Any], errors: List[Dict[str, Any]]) -> None:
    line_no = int(ep.get("__line_no__", -1))
    episode_id = _episode_ref(ep, line_no)

    # P0 required paths and types.
    p0_object_paths = ["initial_state", "initial_state.red", "initial_state.blue", "tactic", "tactic.cmd", "outcome"]
    p0_array_paths = [
        "initial_state.red.aircraft",
        "initial_state.red.ships",
        "initial_state.blue.aircraft",
        "initial_state.blue.ships",
    ]

    if not isinstance(ep.get("episode_id"), str) or not ep.get("episode_id", "").strip():
        _append_error(errors, "P0", line_no, episode_id, "episode_id", "must be non-empty string", ep.get("episode_id"))

    for path in p0_object_paths:
        try:
            value = _get_path(ep, path)
            if not isinstance(value, dict):
                _append_error(errors, "P0", line_no, episode_id, path, "must be object", value)
        except KeyError:
            _append_error(errors, "P0", line_no, episode_id, path, "missing required field")

    for path in p0_array_paths:
        try:
            value = _get_path(ep, path)
            if not isinstance(value, list):
                _append_error(errors, "P0", line_no, episode_id, path, "must be array", value)
        except KeyError:
            _append_error(errors, "P0", line_no, episode_id, path, "missing required field")

    # P0 tactic command fields
    tactic_number_paths = [
        "tactic.cmd.launch_delay_s",
        "tactic.cmd.formation_compactness",
        "tactic.cmd.target_focus_ratio",
        "tactic.cmd.attack_bearing_deg",
        "tactic.cmd.support_level",
    ]
    for path in tactic_number_paths:
        try:
            value = _get_path(ep, path)
            if not _is_number(value):
                _append_error(errors, "P0", line_no, episode_id, path, "must be finite number", value)
        except KeyError:
            _append_error(errors, "P0", line_no, episode_id, path, "missing required field")

    # P0 outcome rules
    try:
        red_win = _get_path(ep, "outcome.red_win")
        if not isinstance(red_win, int) or isinstance(red_win, bool) or red_win not in (0, 1):
            _append_error(errors, "P0", line_no, episode_id, "outcome.red_win", "must be int in {0,1}", red_win)
    except KeyError:
        _append_error(errors, "P0", line_no, episode_id, "outcome.red_win", "missing required field")

    for ratio_path in ["outcome.red_loss_ratio", "outcome.blue_loss_ratio"]:
        try:
            value = _get_path(ep, ratio_path)
            if not _is_number(value):
                _append_error(errors, "P0", line_no, episode_id, ratio_path, "must be finite number", value)
            elif not (0.0 <= float(value) <= 1.0):
                _append_error(errors, "P0", line_no, episode_id, ratio_path, "must be in [0,1]", value)
        except KeyError:
            _append_error(errors, "P0", line_no, episode_id, ratio_path, "missing required field")

    # P1 recommended top-level fields.
    for path in ["timestamp", "seed", "tactic.tactic_id", "outcome.duration_s", "final_state"]:
        try:
            _ = _get_path(ep, path)
        except KeyError:
            _append_error(errors, "P1", line_no, episode_id, path, "missing recommended field")

    # P1 extra sanity.
    for path in ["tactic.cmd.formation_compactness", "tactic.cmd.target_focus_ratio", "tactic.cmd.support_level"]:
        try:
            value = float(_get_path(ep, path))
            if not (0.0 <= value <= 1.0):
                _append_error(errors, "P1", line_no, episode_id, path, "recommended range is [0,1]", value)
        except Exception:
            pass

    try:
        launch_delay = float(_get_path(ep, "tactic.cmd.launch_delay_s"))
        if launch_delay < 0:
            _append_error(errors, "P1", line_no, episode_id, "tactic.cmd.launch_delay_s", "should be >= 0", launch_delay)
    except Exception:
        pass

    try:
        duration_s = _get_path(ep, "outcome.duration_s")
        if not _is_number(duration_s):
            _append_error(errors, "P1", line_no, episode_id, "outcome.duration_s", "must be finite number", duration_s)
        elif float(duration_s) < 0:
            _append_error(errors, "P1", line_no, episode_id, "outcome.duration_s", "must be >= 0", duration_s)
    except KeyError:
        pass

    # Validate per-unit shapes and recommended fields.
    for side in ["red", "blue"]:
        for unit_list_name in ["aircraft", "ships"]:
            path = f"initial_state.{side}.{unit_list_name}"
            try:
                units = _get_path(ep, path)
            except KeyError:
                continue
            if isinstance(units, list):
                _validate_unit_fields(errors, episode_id, line_no, side, unit_list_name, units)


def _should_fail(priority: str, fail_on: str) -> bool:
    return PRIORITY_ORDER[priority] <= PRIORITY_ORDER[fail_on]


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate C++ exported episodes.jsonl/.json with P0/P1/P2 quality gates.")
    parser.add_argument("--input-path", type=Path, required=True, help="Path to episodes .jsonl or .json")
    parser.add_argument(
        "--fail-on",
        type=str,
        choices=["P0", "P1", "P2"],
        default="P0",
        help="Exit non-zero when any error at or above this priority exists. Default: P0",
    )
    parser.add_argument("--max-print", type=int, default=200, help="Max number of error rows to print")
    args = parser.parse_args()

    errors: List[Dict[str, Any]] = []
    total = 0

    try:
        records = list(_iter_json_records(args.input_path))
    except Exception as exc:
        print(f"[FATAL] {exc}")
        raise SystemExit(2)

    for ep in records:
        total += 1
        _validate_episode(ep, errors)

    p0_count = sum(1 for e in errors if e["priority"] == "P0")
    p1_count = sum(1 for e in errors if e["priority"] == "P1")
    p2_count = sum(1 for e in errors if e["priority"] == "P2")

    for e in errors[: max(0, args.max_print)]:
        print(
            f"[{e['priority']}] line={e['line']} episode_id={e['episode_id']} "
            f"field={e['field']} msg={e['message']} value={e.get('value')}"
        )

    if len(errors) > args.max_print:
        print(f"... truncated {len(errors) - args.max_print} additional errors")

    summary = {
        "episodes_total": total,
        "errors_total": len(errors),
        "p0_errors": p0_count,
        "p1_errors": p1_count,
        "p2_errors": p2_count,
        "fail_on": args.fail_on,
    }
    print("Validation summary:", summary)

    should_fail = any(_should_fail(e["priority"], args.fail_on) for e in errors)
    raise SystemExit(1 if should_fail else 0)


if __name__ == "__main__":
    main()

