import argparse
import random
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Tuple

from data_io import write_json, write_jsonl
from real_adapter import adapt_real_payload_to_sample
from tactic_slicer import evaluate_2v1_slices_heuristic, evaluate_2v1_slices_with_model, slice_4v2_to_2v1


def _read_jsonl(path: Path) -> Iterable[Dict]:
    with path.open("r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, start=1):
            line = line.strip()
            if not line:
                continue
            try:
                import json

                yield json.loads(line)
            except Exception as exc:
                raise ValueError(f"Invalid JSONL at line {line_no}: {exc}") from exc


def _read_json(path: Path) -> List[Dict]:
    import json

    with path.open("r", encoding="utf-8") as f:
        payload = json.load(f)
    if isinstance(payload, list):
        return payload
    if isinstance(payload, dict) and "episodes" in payload and isinstance(payload["episodes"], list):
        return payload["episodes"]
    if isinstance(payload, dict) and "red" in payload and "blue" in payload:
        return [
            {
                "episode_id": payload.get("battle_id", "mock-real-episode"),
                "timestamp": payload.get("timestamp"),
                "initial_state": {
                    "red": payload.get("red", {}),
                    "blue": payload.get("blue", {}),
                },
                "tactic": {
                    "tactic_id": payload.get("tactic_id", "REAL_PAYLOAD"),
                    "cmd": payload.get("tactic_cmd", {}),
                },
                "outcome": payload.get("outcome", {"red_win": 0}),
                "final_state": payload.get("final_state", {}),
            }
        ]
    if isinstance(payload, dict):
        return [payload]
    raise ValueError("JSON input must be a list or a dict-like payload.")


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, float(x)))


def _safe_float(x: Any, default: float = 0.0) -> float:
    try:
        return float(x)
    except (TypeError, ValueError):
        return default


def _norm(v: float, lo: float, hi: float) -> float:
    if hi <= lo:
        return 0.0
    return _clamp01((v - lo) / (hi - lo))


def _map_tactic(cmd: Dict[str, Any]) -> List[float]:
    return [
        _norm(_safe_float(cmd.get("launch_delay_s"), 60.0), 0.0, 600.0),
        _norm(_safe_float(cmd.get("formation_compactness"), 0.5), 0.0, 1.0),
        _norm(_safe_float(cmd.get("target_focus_ratio"), 0.5), 0.0, 1.0),
        _norm(abs(_safe_float(cmd.get("attack_bearing_deg"), 45.0)), 0.0, 180.0),
        _norm(_safe_float(cmd.get("support_level"), 0.5), 0.0, 1.0),
    ]


def _extract_rows(container: Any, feature_key: str) -> List[Dict[str, Any]]:
    if isinstance(container, list):
        out: List[Dict[str, Any]] = []
        for i, x in enumerate(container):
            if isinstance(x, dict):
                out.append(x)
            elif isinstance(x, list):
                out.append({"id": f"{feature_key}_{i}", "features": x})
        return out

    if not isinstance(container, dict):
        return []

    # New compressed schema: red_features / blue_features are plain vectors.
    if isinstance(container.get(feature_key), list):
        rows = []
        for i, x in enumerate(container[feature_key]):
            if isinstance(x, dict):
                rows.append(x)
            elif isinstance(x, list):
                rows.append({"id": f"{feature_key}_{i}", "features": x})
        return rows

    rows: List[Dict[str, Any]] = []
    for k in ("units", "aircraft", "ships", "red", "blue"):
        v = container.get(k)
        if isinstance(v, list):
            rows.extend(_extract_rows(v, feature_key))
        elif isinstance(v, dict):
            rows.extend(_extract_rows(v, feature_key))
    return rows


def _extract_initial_rows(ep: Dict[str, Any], side: str) -> List[Dict[str, Any]]:
    initial = ep.get("initial_state", {})
    feature_key = f"{side}_features"
    side_obj = initial.get(side, initial)

    rows = _extract_rows(side_obj, feature_key)
    if rows:
        return rows

    # Compatibility: direct real payload fields at root.
    return _extract_rows(ep.get(side, {}), feature_key)


def _extract_final_rows(ep: Dict[str, Any], side: str) -> List[Dict[str, Any]]:
    final_state = ep.get("final_state", {})
    feature_key = f"{side}_features"
    side_obj = final_state.get(side, final_state)
    return _extract_rows(side_obj, feature_key)


def _to_sample_from_initial_rows(red_rows: List[Dict[str, Any]], blue_rows: List[Dict[str, Any]], tactic_cmd: Dict[str, Any]) -> Dict[str, Any]:
    payload = {
        "red": {"units": red_rows},
        "blue": {"units": blue_rows},
        "tactic_cmd": tactic_cmd,
    }
    return adapt_real_payload_to_sample(payload)


def _parse_final_2d(units: List[Dict[str, Any]]) -> List[Tuple[float, float, Optional[str]]]:
    out: List[Tuple[float, float, Optional[str]]] = []
    for i, u in enumerate(units):
        feat = u.get("features") if isinstance(u, dict) else None
        if isinstance(feat, list) and len(feat) >= 6:
            # Legacy fallback where alive and missile were embedded in long feature vectors.
            missile = max(0.0, _safe_float(feat[2], 0.0))
            alive = _clamp01(_safe_float(feat[-1], 0.0))
        elif isinstance(feat, list) and len(feat) >= 2:
            missile = max(0.0, _safe_float(feat[0], 0.0))
            alive = _clamp01(_safe_float(feat[1], 0.0))
        else:
            missile = max(0.0, _safe_float(u.get("missile"), 0.0))
            alive = _clamp01(_safe_float(u.get("alive"), 0.0))
        unit_id = u.get("id") if isinstance(u, dict) else f"unit_{i}"
        out.append((missile, alive, unit_id))
    return out


def _align_final_targets(initial_units: List[Dict[str, Any]], final_rows: List[Dict[str, Any]]) -> Tuple[List[float], List[float]]:
    final_parsed = _parse_final_2d(final_rows)
    by_id: Dict[str, Tuple[float, float]] = {}
    for missile, alive, unit_id in final_parsed:
        if unit_id:
            by_id[str(unit_id)] = (missile, alive)

    missiles: List[float] = []
    alive_vals: List[float] = []
    for i, u in enumerate(initial_units):
        uid = u.get("id")
        if uid is not None and str(uid) in by_id:
            m, a = by_id[str(uid)]
        elif i < len(final_parsed):
            m, a, _ = final_parsed[i]
        else:
            # Missing final rows: fallback to unchanged missile and alive=1 for compatibility.
            m = _safe_float(u.get("features", [0.0, 0.0, 0.0])[2], 0.0)
            a = 1.0
        missiles.append(max(0.0, float(m)))
        alive_vals.append(_clamp01(float(a)))
    return missiles, alive_vals


def _float32_compat_list(values: List[float]) -> List[float]:
    # Keep ingest side torch-independent, but enforce numeric float labels
    # so dataset.py can always build torch.float32 tensors safely.
    return [float(x) for x in values]


def _validate_required(ep: Dict[str, Any]) -> bool:
    if "initial_state" not in ep:
        return False
    if "outcome" not in ep or "red_win" not in ep.get("outcome", {}):
        return False
    return True


def _slicer_eval(macro_sample: Dict[str, Any], strategy: str, model: Optional[Any]) -> Dict[str, Any]:
    if model is not None:
        return evaluate_2v1_slices_with_model(model, macro_sample, strategy=strategy)
    return evaluate_2v1_slices_heuristic(macro_sample, strategy=strategy)


def _slice_labels_by_indices(labels: Dict[str, Any], red_idx: List[int], blue_idx: int) -> Dict[str, Any]:
    return {
        "red_final_alive": [labels["red_final_alive"][i] for i in red_idx],
        "red_final_missile": [labels["red_final_missile"][i] for i in red_idx],
        "blue_final_alive": [labels["blue_final_alive"][blue_idx]],
        "blue_final_missile": [labels["blue_final_missile"][blue_idx]],
    }


def _convert_episode(
    ep: Dict[str, Any],
    expand_4v2: bool = False,
    slicer_strategy: str = "distance_threat",
    slicer_model: Optional[Any] = None,
    evaluate_slices: bool = False,
) -> List[Dict[str, Any]]:
    red_rows = _extract_initial_rows(ep, "red")
    blue_rows = _extract_initial_rows(ep, "blue")

    tactic_obj = ep.get("tactic", {}) if isinstance(ep.get("tactic"), dict) else {}
    tactic_cmd = tactic_obj.get("cmd", ep.get("tactic_cmd", {}))
    sample = _to_sample_from_initial_rows(red_rows, blue_rows, tactic_cmd)

    red_final_rows = _extract_final_rows(ep, "red")
    blue_final_rows = _extract_final_rows(ep, "blue")

    red_final_missile, red_final_alive = _align_final_targets(sample["red_units"], red_final_rows)
    blue_final_missile, blue_final_alive = _align_final_targets(sample["blue_units"], blue_final_rows)

    red_win = 1.0 if _safe_float(ep.get("outcome", {}).get("red_win"), 0.0) >= 0.5 else 0.0
    red_loss = _clamp01(1.0 - (sum(red_final_alive) / max(len(red_final_alive), 1)))
    blue_loss = _clamp01(1.0 - (sum(blue_final_alive) / max(len(blue_final_alive), 1)))

    labels = {
        "p_red_win": _clamp01(_safe_float(ep.get("outcome", {}).get("p_red_win"), red_win)),
        "red_win": red_win,
        "red_loss": red_loss,
        "blue_loss": blue_loss,
        "red_final_alive": _float32_compat_list(red_final_alive),
        "blue_final_alive": _float32_compat_list(blue_final_alive),
        "red_final_missile": _float32_compat_list(red_final_missile),
        "blue_final_missile": _float32_compat_list(blue_final_missile),
    }

    base_record = {
        "red_units": sample["red_units"],
        "blue_units": sample["blue_units"],
        "tactic": _map_tactic(tactic_cmd),
        "labels": labels,
        "meta": {
            "episode_id": ep.get("episode_id", ep.get("task_id")),
            "task_id": ep.get("task_id"),
            "tactic_id": tactic_obj.get("tactic_id", ep.get("tactic_id")),
            "seed": ep.get("seed"),
            "timestamp": ep.get("timestamp"),
            "source_mode": ep.get("encounter_spec", {}).get("mode", "unknown"),
        },
    }

    if not expand_4v2 or not (len(sample["red_units"]) == 4 and len(sample["blue_units"]) == 2):
        return [base_record]

    macro = {
        "red_units": base_record["red_units"],
        "blue_units": base_record["blue_units"],
        "tactic": base_record["tactic"],
    }
    slices = slice_4v2_to_2v1(macro, strategy=slicer_strategy)
    eval_result = _slicer_eval(macro, slicer_strategy, slicer_model) if evaluate_slices else None

    records: List[Dict[str, Any]] = []
    for i, local in enumerate(slices):
        red_idx = list(local.get("slice_meta", {}).get("red_indices", [0, 1]))
        blue_idx = int(local.get("slice_meta", {}).get("blue_index", 0))
        sliced_labels = _slice_labels_by_indices(labels, red_idx, blue_idx)
        rec = {
            "red_units": local["red_units"],
            "blue_units": local["blue_units"],
            "tactic": local["tactic"],
            "labels": {
                **labels,
                **sliced_labels,
                "p_red_win": (
                    eval_result["per_slice_red_win_prob"][i]
                    if eval_result is not None
                    else labels["p_red_win"]
                ),
            },
            "meta": {
                **base_record["meta"],
                "slice_index": i,
                "slice_meta": local.get("slice_meta", {}),
                "slice_eval": eval_result if i == 0 and eval_result is not None else None,
            },
        }
        records.append(rec)
    return records


def _split_records(records: List[Dict[str, Any]], train_ratio: float, val_ratio: float, seed: int):
    rng = random.Random(seed)
    rng.shuffle(records)

    n = len(records)
    n_train = int(n * train_ratio)
    n_val = int(n * val_ratio)

    if n >= 3 and n_val == 0 and val_ratio > 0.0:
        n_val = 1
    if n - n_train - n_val == 0 and n >= 2:
        if n_train > 1:
            n_train -= 1
        elif n_val > 1:
            n_val -= 1

    train = records[:n_train]
    val = records[n_train : n_train + n_val]
    test = records[n_train + n_val :]
    return train, val, test


def main() -> None:
    parser = argparse.ArgumentParser(description="Ingest C++ episodes and build train/val/test jsonl.")
    parser.add_argument("--input-path", type=Path, required=True, help="Path to exported .jsonl or .json")
    parser.add_argument("--out-dir", type=Path, required=True, help="Output root dir, e.g. agent_mvp/data_real")
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--train-ratio", type=float, default=0.7)
    parser.add_argument("--val-ratio", type=float, default=0.15)
    parser.add_argument("--strict", action="store_true", help="Fail on first invalid episode")
    parser.add_argument("--expand-4v2", action="store_true", help="Expand 4v2 episodes into two 2v1 slices")
    parser.add_argument("--slicer-strategy", type=str, default="distance_threat", choices=["distance_threat", "distance", "threat"])
    parser.add_argument("--evaluate-slices", action="store_true", help="Evaluate 4v2 slices with heuristic or model")
    parser.add_argument("--model-path", type=Path, default=None, help="Optional model checkpoint for model-based slicer eval")
    args = parser.parse_args()

    if not (0.0 < args.train_ratio < 1.0 and 0.0 <= args.val_ratio < 1.0 and args.train_ratio + args.val_ratio < 1.0):
        raise ValueError("Invalid split ratios. Need 0<train<1, 0<=val<1 and train+val<1.")

    raw_episodes = list(_read_jsonl(args.input_path)) if args.input_path.suffix.lower() == ".jsonl" else _read_json(args.input_path)

    slicer_model = None
    if args.evaluate_slices and args.model_path is not None:
        try:
            import torch

            from model import MacroEvalNet

            slicer_model = MacroEvalNet()
            slicer_model.load_state_dict(torch.load(args.model_path, map_location="cpu"), strict=False)
            slicer_model.eval()
        except Exception as exc:
            if args.strict:
                raise RuntimeError(f"Failed to load model for slicer evaluation: {exc}") from exc
            print(f"[WARN] Falling back to heuristic slicer eval. Model load failed: {exc}")

    converted: List[Dict[str, Any]] = []
    invalid_count = 0

    for idx, ep in enumerate(raw_episodes):
        if not _validate_required(ep):
            invalid_count += 1
            if args.strict:
                raise ValueError(f"Episode at index {idx} missing required fields.")
            continue

        try:
            converted.extend(
                _convert_episode(
                    ep,
                    expand_4v2=args.expand_4v2,
                    slicer_strategy=args.slicer_strategy,
                    slicer_model=slicer_model,
                    evaluate_slices=args.evaluate_slices,
                )
            )
        except Exception:
            invalid_count += 1
            if args.strict:
                raise

    train, val, test = _split_records(converted, args.train_ratio, args.val_ratio, args.seed)

    write_jsonl(args.out_dir / "raw" / "episodes.jsonl", raw_episodes)
    write_jsonl(args.out_dir / "processed" / "train.jsonl", train)
    write_jsonl(args.out_dir / "processed" / "val.jsonl", val)
    write_jsonl(args.out_dir / "processed" / "test.jsonl", test)

    summary = {
        "episodes_total": len(raw_episodes),
        "episodes_valid": len(converted),
        "episodes_invalid": invalid_count,
        "expand_4v2": args.expand_4v2,
        "evaluate_slices": args.evaluate_slices,
        "slicer_strategy": args.slicer_strategy,
        "train": len(train),
        "val": len(val),
        "test": len(test),
        "seed": args.seed,
        "train_ratio": args.train_ratio,
        "val_ratio": args.val_ratio,
    }
    write_json(args.out_dir / "processed" / "summary.json", summary)
    print("Ingest finished:", summary)


if __name__ == "__main__":
    main()


