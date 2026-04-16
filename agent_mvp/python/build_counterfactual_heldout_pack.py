import argparse
import random
from collections import defaultdict
from pathlib import Path
from typing import Any, DefaultDict, Dict, List, Sequence, Tuple

from data_io import write_json, write_jsonl
from same_initial_signature import canonical_signature_from_record
from train_world_model import load_split_records


def _tactic_combo_key(record: Dict[str, Any]) -> str:
    meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
    sampling_meta = meta.get("sampling_meta", {}) if isinstance(meta.get("sampling_meta", {}), dict) else {}
    combo = sampling_meta.get("tactic_combo_key")
    if combo is not None:
        return str(combo)

    red_cond = record.get("red_tactic_condition", {}) if isinstance(record.get("red_tactic_condition", {}), dict) else {}
    blue_cond = record.get("blue_tactic_condition", {}) if isinstance(record.get("blue_tactic_condition", {}), dict) else {}
    return f"{red_cond.get('id', 'red_unknown')}__{blue_cond.get('id', 'blue_unknown')}"


def _collect_records(data_dir: Path, splits: Sequence[str]) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    for split_name in splits:
        split_records = load_split_records(data_dir, split_name)
        if split_records:
            records.extend(split_records)
    return records


def _group_same_initial(
    records: Sequence[Dict[str, Any]],
    horizons: Sequence[str],
    state_step: int,
) -> Dict[Tuple[str, str, int], List[Dict[str, Any]]]:
    requested_horizons = {str(item) for item in horizons}
    grouped: DefaultDict[Tuple[str, str, int], List[Dict[str, Any]]] = defaultdict(list)

    for record in records:
        meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
        record_state_step = int(meta.get("state_step", -1))
        if record_state_step != int(state_step):
            continue
        if requested_horizons and str(record.get("horizon")) not in requested_horizons:
            continue

        key = (
            canonical_signature_from_record(record),
            str(record.get("horizon")),
            record_state_step,
        )
        grouped[key].append(record)

    return dict(grouped)


def _eligible_groups(
    grouped: Dict[Tuple[str, str, int], List[Dict[str, Any]]]
) -> Dict[Tuple[str, str, int], List[Dict[str, Any]]]:
    output: Dict[Tuple[str, str, int], List[Dict[str, Any]]] = {}
    for group_key, records in grouped.items():
        combos = {_tactic_combo_key(record) for record in records}
        if len(records) >= 2 and len(combos) >= 2:
            output[group_key] = records
    return output


def _split_group_keys(
    keys: Sequence[Tuple[str, str, int]],
    heldout_ratio: float,
    seed: int,
    min_heldout_groups: int,
) -> Tuple[List[Tuple[str, str, int]], List[Tuple[str, str, int]]]:
    values = list(keys)
    rng = random.Random(seed)
    rng.shuffle(values)

    if not values:
        return [], []

    raw_heldout = int(round(len(values) * heldout_ratio))
    heldout_count = max(min_heldout_groups, raw_heldout)
    heldout_count = min(max(0, heldout_count), len(values))
    heldout = values[:heldout_count]
    visible = values[heldout_count:]
    return visible, heldout


def _base_group_key(group_key: Tuple[str, str, int]) -> Tuple[str, int]:
    return (str(group_key[0]), int(group_key[2]))


def _build_base_group_map(
    eligible_groups: Dict[Tuple[str, str, int], List[Dict[str, Any]]]
) -> Dict[Tuple[str, int], List[Tuple[str, str, int]]]:
    output: DefaultDict[Tuple[str, int], List[Tuple[str, str, int]]] = defaultdict(list)
    for group_key in eligible_groups:
        output[_base_group_key(group_key)].append(group_key)
    return dict(output)


def _preassigned_split_from_group(
    records: Sequence[Dict[str, Any]],
    split_key: str,
) -> str:
    values = set()
    for record in records:
        meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
        sampling_meta = meta.get("sampling_meta", {}) if isinstance(meta.get("sampling_meta", {}), dict) else {}
        value = str(sampling_meta.get(split_key, "")).strip()
        if value:
            values.add(value)
    if len(values) == 1:
        return list(values)[0]
    return ""


def _split_base_groups(
    base_groups: Dict[Tuple[str, int], List[Tuple[str, str, int]]],
    eligible_groups: Dict[Tuple[str, str, int], List[Dict[str, Any]]],
    heldout_ratio: float,
    seed: int,
    min_heldout_groups: int,
    respect_preassigned_split: bool,
    preassigned_split_key: str,
) -> Tuple[List[Tuple[str, str, int]], List[Tuple[str, str, int]], Dict[str, Any]]:
    base_keys = sorted(base_groups.keys())
    diagnostics: Dict[str, Any] = {
        "respect_preassigned_split": bool(respect_preassigned_split),
        "preassigned_split_key": str(preassigned_split_key),
        "preassigned_train_visible_base_groups": 0,
        "preassigned_heldout_base_groups": 0,
        "random_assigned_base_groups": 0,
    }

    visible_base: List[Tuple[str, int]] = []
    heldout_base: List[Tuple[str, int]] = []
    undecided_base: List[Tuple[str, int]] = []

    for base_key in base_keys:
        if not respect_preassigned_split:
            undecided_base.append(base_key)
            continue

        all_records: List[Dict[str, Any]] = []
        for group_key in base_groups[base_key]:
            all_records.extend(eligible_groups[group_key])
        split_value = _preassigned_split_from_group(all_records, split_key=preassigned_split_key)
        if split_value == "heldout":
            heldout_base.append(base_key)
            diagnostics["preassigned_heldout_base_groups"] += 1
        elif split_value == "train_visible":
            visible_base.append(base_key)
            diagnostics["preassigned_train_visible_base_groups"] += 1
        else:
            undecided_base.append(base_key)

    rng = random.Random(seed)
    rng.shuffle(undecided_base)

    target_heldout = int(round(len(base_keys) * heldout_ratio))
    target_heldout = max(min_heldout_groups, target_heldout)
    target_heldout = min(max(0, target_heldout), len(base_keys))
    missing_heldout = max(0, target_heldout - len(heldout_base))

    heldout_base.extend(undecided_base[:missing_heldout])
    visible_base.extend(undecided_base[missing_heldout:])
    diagnostics["random_assigned_base_groups"] = len(undecided_base)

    visible_keys: List[Tuple[str, str, int]] = []
    heldout_keys: List[Tuple[str, str, int]] = []
    for base_key in visible_base:
        visible_keys.extend(base_groups.get(base_key, []))
    for base_key in heldout_base:
        heldout_keys.extend(base_groups.get(base_key, []))

    return sorted(visible_keys), sorted(heldout_keys), diagnostics


def _flatten_group_records(
    grouped: Dict[Tuple[str, str, int], List[Dict[str, Any]]],
    keys: Sequence[Tuple[str, str, int]],
    split_name: str,
) -> List[Dict[str, Any]]:
    output: List[Dict[str, Any]] = []
    for key in keys:
        for record in grouped[key]:
            cloned = dict(record)
            meta = dict(cloned.get("meta", {}))
            meta["counterfactual_pack_split"] = split_name
            meta["counterfactual_same_initial_signature"] = key[0]
            meta["counterfactual_same_initial_base_group"] = f"{key[0]}::step{int(key[2])}"
            cloned["meta"] = meta
            output.append(cloned)
    return output


def _coverage_summary(records: Sequence[Dict[str, Any]]) -> Dict[str, Any]:
    combos = sorted({_tactic_combo_key(record) for record in records})
    episodes = sorted({str((record.get("meta", {}) or {}).get("episode_id", "")) for record in records if (record.get("meta", {}) or {}).get("episode_id")})
    return {
        "num_records": len(records),
        "num_tactic_combos": len(combos),
        "tactic_combos": combos,
        "num_episodes": len(episodes),
        "episodes": episodes,
    }


def _build_markdown(manifest: Dict[str, Any]) -> str:
    lines: List[str] = []
    lines.append("# Held-out Counterfactual Pack Summary")
    lines.append("")
    lines.append("## Grouping")
    lines.append(f"- same_initial_signature: {manifest['grouping']['same_initial_signature']}")
    lines.append(f"- group_key_fields: {manifest['grouping']['group_key_fields']}")
    lines.append("")
    lines.append("## Inputs")
    lines.append(f"- source_data_dir: {manifest['source_data_dir']}")
    lines.append(f"- source_splits: {manifest['source_splits']}")
    lines.append(f"- horizons: {manifest['horizons']}")
    lines.append(f"- state_step: {manifest['state_step']}")
    lines.append("")
    lines.append("## Group Counts")
    lines.append(f"- total_candidate_groups: {manifest['total_candidate_groups']}")
    lines.append(f"- eligible_same_initial_groups: {manifest['eligible_groups']}")
    lines.append(f"- train_visible_groups: {manifest['train_visible_groups']}")
    lines.append(f"- heldout_groups: {manifest['heldout_groups']}")
    lines.append("")
    lines.append("## Split Coverage")
    lines.append(f"- train_visible_records: {manifest['train_visible']['num_records']}")
    lines.append(f"- heldout_records: {manifest['heldout']['num_records']}")
    lines.append(f"- train_visible_tactic_combos: {manifest['train_visible']['num_tactic_combos']}")
    lines.append(f"- heldout_tactic_combos: {manifest['heldout']['num_tactic_combos']}")
    lines.append("")
    lines.append("## Boundary")
    lines.append("- train_visible_cf.jsonl is allowed for training/counterfactual finetune.")
    lines.append("- heldout_cf.jsonl is eval-only and must not be used in training/finetune.")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Build held-out same-initial/different-tactic pack.")
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--source-splits", type=str, default="train,val,test")
    parser.add_argument("--horizons", type=str, default="20,terminal")
    parser.add_argument("--state-step", type=int, default=0)
    parser.add_argument("--heldout-ratio", type=float, default=0.3)
    parser.add_argument("--min-heldout-groups", type=int, default=5)
    parser.add_argument("--seed", type=int, default=7)
    parser.add_argument("--respect-preassigned-split", action="store_true")
    parser.add_argument("--preassigned-split-key", type=str, default="counterfactual_pack_split")
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    source_splits = [token.strip() for token in args.source_splits.split(",") if token.strip()]
    horizons = [token.strip() for token in args.horizons.split(",") if token.strip()]

    source_records = _collect_records(args.data_dir, source_splits)
    grouped = _group_same_initial(source_records, horizons=horizons, state_step=args.state_step)
    eligible = _eligible_groups(grouped)
    base_groups = _build_base_group_map(eligible)

    visible_keys, heldout_keys, split_diagnostics = _split_base_groups(
        base_groups=base_groups,
        eligible_groups=eligible,
        heldout_ratio=args.heldout_ratio,
        seed=args.seed,
        min_heldout_groups=args.min_heldout_groups,
        respect_preassigned_split=bool(args.respect_preassigned_split),
        preassigned_split_key=args.preassigned_split_key,
    )

    train_visible_records = _flatten_group_records(eligible, visible_keys, split_name="train_visible")
    heldout_records = _flatten_group_records(eligible, heldout_keys, split_name="heldout")

    processed_dir = args.out_dir / "processed"
    write_jsonl(processed_dir / "train_visible_cf.jsonl", train_visible_records)
    write_jsonl(processed_dir / "heldout_cf.jsonl", heldout_records)

    manifest = {
        "source_data_dir": str(args.data_dir),
        "source_splits": source_splits,
        "horizons": horizons,
        "state_step": int(args.state_step),
        "grouping": {
            "same_initial_signature": "canonical_signature_from_record(record['state_t'])",
            "group_key_fields": ["state_signature", "horizon", "state_step"],
            "different_tactic_key": "tactic_combo_key",
        },
        "total_source_records": len(source_records),
        "total_candidate_groups": len(grouped),
        "eligible_groups": len(eligible),
        "eligible_base_groups": len(base_groups),
        "train_visible_groups": len(visible_keys),
        "heldout_groups": len(heldout_keys),
        "train_visible_base_groups": len({_base_group_key(key) for key in visible_keys}),
        "heldout_base_groups": len({_base_group_key(key) for key in heldout_keys}),
        "split_diagnostics": split_diagnostics,
        "train_visible": _coverage_summary(train_visible_records),
        "heldout": _coverage_summary(heldout_records),
        "output": {
            "train_visible_path": str(processed_dir / "train_visible_cf.jsonl"),
            "heldout_path": str(processed_dir / "heldout_cf.jsonl"),
        },
    }

    write_json(args.out_dir / "manifest.json", manifest)
    (args.out_dir / "summary.md").write_text(_build_markdown(manifest), encoding="utf-8")

    print(
        "Held-out pack built | "
        f"eligible_groups={manifest['eligible_groups']} | "
        f"train_visible_groups={manifest['train_visible_groups']} | "
        f"heldout_groups={manifest['heldout_groups']}"
    )


if __name__ == "__main__":
    main()
