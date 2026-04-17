import argparse
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

from data_io import read_json, read_jsonl, write_json, write_jsonl


PRIMARY_BUCKETS: Tuple[str, ...] = (
    "terminal_red_first_kill_outcome_confusion",
    "terminal_blue_objective_without_first_kill",
)
EXTENDED_BUCKETS: Tuple[str, ...] = PRIMARY_BUCKETS + (
    "terminal_or_late_window_without_any_first_kill_remaining",
)
NON_DECISIVE_TERMINATION_REASONS = {"safety_limit", "none", "timeout", "time_limit"}


def sample_id_from_record(record: Dict) -> str:
    meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
    episode_id = str(meta.get("episode_id", ""))
    horizon = str(record.get("horizon"))
    t_index = int(meta.get("t_index", -1))
    target_step = int(meta.get("target_step", -1))
    return f"{episode_id}|{horizon}|{t_index}|{target_step}"


def tactic_combo_key(record: Dict) -> str:
    meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
    sampling_meta = meta.get("sampling_meta", {}) if isinstance(meta.get("sampling_meta", {}), dict) else {}
    combo = sampling_meta.get("tactic_combo_key")
    if combo is not None:
        return str(combo)

    red_cond = record.get("red_tactic_condition", {}) if isinstance(record.get("red_tactic_condition", {}), dict) else {}
    blue_cond = record.get("blue_tactic_condition", {}) if isinstance(record.get("blue_tactic_condition", {}), dict) else {}
    return f"{red_cond.get('id', 'red_unknown')}__{blue_cond.get('id', 'blue_unknown')}"


def effective_termination(record: Dict) -> bool:
    meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
    reason = str(meta.get("target_termination_reason", "")).strip().lower()
    return bool(reason) and reason not in NON_DECISIVE_TERMINATION_REASONS


def classify_terminal_confusion_bucket(record: Dict) -> str:
    if str(record.get("horizon")) != "terminal":
        return ""

    event = record.get("target_event", {}) if isinstance(record.get("target_event", {}), dict) else {}
    red_first_kill = bool(event.get("red_first_kill_flag", False))
    blue_first_kill = bool(event.get("blue_first_kill_flag", False))
    red_objective = bool(event.get("red_objective_complete_flag", False))
    blue_objective = bool(event.get("blue_objective_complete_flag", False))
    is_effective_termination = effective_termination(record)

    if is_effective_termination and red_first_kill and red_objective and not blue_first_kill:
        return "terminal_red_first_kill_outcome_confusion"
    if is_effective_termination and blue_objective and not blue_first_kill and not red_first_kill:
        return "terminal_blue_objective_without_first_kill"
    if (
        not red_first_kill
        and not blue_first_kill
        and not is_effective_termination
        and not red_objective
        and not blue_objective
    ):
        return "terminal_or_late_window_without_any_first_kill_remaining"
    return ""


def coverage(records: Sequence[Dict]) -> Dict:
    episodes = sorted(
        {
            str((record.get("meta", {}) or {}).get("episode_id", ""))
            for record in records
            if (record.get("meta", {}) or {}).get("episode_id")
        }
    )
    horizons = sorted({str(record.get("horizon")) for record in records})
    tactic_combos = sorted({tactic_combo_key(record) for record in records})
    buckets = {}
    for record in records:
        meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
        bucket = str(meta.get("terminal_confusion_bucket", "unlabeled"))
        buckets[bucket] = buckets.get(bucket, 0) + 1
    return {
        "num_records": len(records),
        "num_episodes": len(episodes),
        "episodes": episodes,
        "horizons": horizons,
        "num_tactic_combos": len(tactic_combos),
        "tactic_combos": tactic_combos,
        "bucket_counts": buckets,
    }


def _clone_with_bucket(record: Dict, bucket: str, primary: bool) -> Dict:
    cloned = dict(record)
    meta = dict(cloned.get("meta", {}))
    meta["terminal_confusion_bucket"] = str(bucket)
    meta["terminal_confusion_primary"] = bool(primary)
    cloned["meta"] = meta
    return cloned


def _bucket_counts(records: Sequence[Dict]) -> Dict[str, int]:
    counts: Dict[str, int] = {}
    for record in records:
        meta = record.get("meta", {}) if isinstance(record.get("meta", {}), dict) else {}
        bucket = str(meta.get("terminal_confusion_bucket", "unlabeled"))
        counts[bucket] = counts.get(bucket, 0) + 1
    return counts


def build_markdown(manifest: Dict) -> str:
    lines: List[str] = []
    lines.append("# Terminal Confusion Pack Summary")
    lines.append("")
    lines.append("## Scope")
    lines.append(f"- baseline_model: {manifest['baseline_model']}")
    lines.append(f"- source_pack: {manifest['source_pack']}")
    lines.append(f"- source_split: {manifest['source_split']}")
    lines.append("- intended_use: eval-only targeted pack for terminal side-confusion regressions")
    lines.append("")
    lines.append("## Focus Buckets")
    for bucket, count in manifest["focus_bucket_counts"].items():
        lines.append(f"- `{bucket}`: {count}")
    lines.append("")
    lines.append("## Coverage")
    primary = manifest["primary_pack"]
    lines.append(f"- primary_records: {primary['num_records']}")
    lines.append(f"- primary_episodes: {primary['num_episodes']}")
    lines.append(f"- primary_tactic_combos: {primary['num_tactic_combos']}")
    extended = manifest["extended_pack"]
    lines.append(f"- extended_records: {extended['num_records']}")
    lines.append(f"- extended_episodes: {extended['num_episodes']}")
    lines.append("")
    lines.append("## How To Use")
    lines.append("- Use `terminal_confusion_primary_cf.jsonl` as the main regression set for terminal blue-side confusion.")
    lines.append("- Use `terminal_confusion_extended_cf.jsonl` when you also want to track no-first-kill-remaining failures.")
    lines.append("- Do not train on this pack directly; compare future target-definition variants against it.")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Build terminal-confusion-focused eval pack.")
    parser.add_argument("--heldout-path", type=Path, required=True)
    parser.add_argument("--diagnosis-json", type=Path, required=True)
    parser.add_argument("--cases-json", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    args = parser.parse_args()

    diagnosis = read_json(args.diagnosis_json)
    cases_by_bucket = read_json(args.cases_json)
    heldout_records = read_jsonl(args.heldout_path)

    primary_records: List[Dict] = []
    extended_records: List[Dict] = []
    for record in heldout_records:
        bucket = classify_terminal_confusion_bucket(record)
        if not bucket:
            continue
        is_primary = bucket in PRIMARY_BUCKETS
        cloned = _clone_with_bucket(record, bucket=bucket, primary=is_primary)
        if bucket in EXTENDED_BUCKETS:
            extended_records.append(cloned)
        if is_primary:
            primary_records.append(cloned)

    processed_dir = args.out_dir / "processed"
    primary_path = processed_dir / "terminal_confusion_primary_cf.jsonl"
    extended_path = processed_dir / "terminal_confusion_extended_cf.jsonl"
    write_jsonl(primary_path, primary_records)
    write_jsonl(extended_path, extended_records)

    manifest = {
        "baseline_model": "phase10_8_firstkill_fix",
        "source_pack": str(args.heldout_path.parent.parent),
        "source_split": "heldout_cf",
        "source_files": {
            "heldout_path": str(args.heldout_path),
            "diagnosis_json": str(args.diagnosis_json),
            "cases_json": str(args.cases_json),
        },
        "focus_buckets": list(PRIMARY_BUCKETS),
        "focus_bucket_counts": _bucket_counts(primary_records),
        "extended_bucket_counts": _bucket_counts(extended_records),
        "primary_pack": {
            **coverage(primary_records),
            "path": str(primary_path),
        },
        "extended_pack": {
            **coverage(extended_records),
            "path": str(extended_path),
        },
        "diagnosis_references": {
            "blue_false_positive_count": diagnosis.get("blue_false_positive_summary", {}).get("count"),
            "primary_bucket_distribution": diagnosis.get("primary_bucket_distribution", {}),
            "example_cases_by_bucket": {key: len(value) for key, value in cases_by_bucket.items()},
        },
        "consumption_rules": {
            "use_for_training": False,
            "use_for_eval": True,
            "primary_use": "terminal blue-side false-positive regression",
            "comparison_rule": "future target-definition variants should reduce terminal confusion without degrading 10.8 strict calibration",
        },
    }

    write_json(args.out_dir / "manifest.json", manifest)
    (args.out_dir / "summary.md").write_text(build_markdown(manifest), encoding="utf-8")

    print(
        "Terminal confusion pack built | "
        f"primary={len(primary_records)} | extended={len(extended_records)}"
    )


if __name__ == "__main__":
    main()
