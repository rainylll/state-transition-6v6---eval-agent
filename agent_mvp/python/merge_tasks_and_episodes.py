import argparse
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Tuple


def read_jsonl(path: Path) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as f:
        for line_no, line in enumerate(f, start=1):
            raw = line.strip()
            if not raw:
                continue
            try:
                payload = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise ValueError(f"Invalid JSON at line {line_no} in {path}: {exc}") from exc
            if not isinstance(payload, dict):
                raise ValueError(f"Line {line_no} in {path} is not a JSON object.")
            records.append(payload)
    return records


def write_jsonl(path: Path, records: Iterable[Dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for record in records:
            f.write(json.dumps(record, ensure_ascii=False) + "\n")


def index_by_task_id(
    records: List[Dict[str, Any]],
) -> Tuple[Dict[str, Dict[str, Any]], List[str], int, int]:
    indexed: Dict[str, Dict[str, Any]] = {}
    ordered_ids: List[str] = []
    duplicate_count = 0
    missing_task_id_count = 0

    for record in records:
        task_id = record.get("task_id")
        if not isinstance(task_id, str) or not task_id:
            missing_task_id_count += 1
            continue

        if task_id not in indexed:
            ordered_ids.append(task_id)
        else:
            duplicate_count += 1

        indexed[task_id] = record

    return indexed, ordered_ids, duplicate_count, missing_task_id_count


def merge_records(
    tasks_by_id: Dict[str, Dict[str, Any]],
    task_order: List[str],
    episodes_by_id: Dict[str, Dict[str, Any]],
) -> List[Dict[str, Any]]:
    merged: List[Dict[str, Any]] = []
    for task_id in task_order:
        episode = episodes_by_id.get(task_id)
        if episode is None:
            continue

        task = tasks_by_id[task_id]
        merged.append(
            {
                "task_id": task_id,
                "tactic_id": task.get("tactic_id"),
                "initial_state": task.get("initial_state", {}),
                "outcome": episode.get("outcome", {}),
                "final_state": episode.get("final_state", {}),
            }
        )
    return merged


def build_default_path(*parts: str) -> Path:
    repo_root = Path(__file__).resolve().parents[2]
    return repo_root.joinpath(*parts)


def main() -> None:
    parser = argparse.ArgumentParser(description="Merge simulation_tasks.jsonl and episodes.jsonl by task_id.")
    parser.add_argument(
        "--tasks-path",
        type=Path,
        default=build_default_path("agent_mvp", "data_real", "raw", "simulation_tasks.jsonl"),
        help="Path to simulation_tasks.jsonl",
    )
    parser.add_argument(
        "--episodes-path",
        type=Path,
        default=build_default_path("agent_mvp", "data_real", "raw", "episodes.jsonl"),
        help="Path to episodes.jsonl",
    )
    parser.add_argument(
        "--out-path",
        type=Path,
        default=build_default_path("agent_mvp", "data_real", "raw", "episodes_merged.jsonl"),
        help="Path to merged output jsonl",
    )
    args = parser.parse_args()

    tasks = read_jsonl(args.tasks_path)
    episodes = read_jsonl(args.episodes_path)

    tasks_by_id, task_order, task_duplicates, task_missing_ids = index_by_task_id(tasks)
    episodes_by_id, _, episode_duplicates, episode_missing_ids = index_by_task_id(episodes)

    task_ids = set(tasks_by_id.keys())
    episode_ids = set(episodes_by_id.keys())
    matched_ids = task_ids & episode_ids
    missing_in_tasks_ids = episode_ids - task_ids
    missing_in_episodes_ids = task_ids - episode_ids

    merged = merge_records(tasks_by_id, task_order, episodes_by_id)
    write_jsonl(args.out_path, merged)

    stats = {
        "tasks_total": len(task_ids),
        "episodes_total": len(episode_ids),
        "matched": len(matched_ids),
        "missing_in_tasks": len(missing_in_tasks_ids),
        "missing_in_episodes": len(missing_in_episodes_ids),
    }

    print("merge_stats:", json.dumps(stats, ensure_ascii=False))
    if task_duplicates or episode_duplicates or task_missing_ids or episode_missing_ids:
        extra = {
            "task_duplicates": task_duplicates,
            "episode_duplicates": episode_duplicates,
            "task_missing_task_id": task_missing_ids,
            "episode_missing_task_id": episode_missing_ids,
        }
        print("merge_warnings:", json.dumps(extra, ensure_ascii=False))

    print(f"merged_output: {args.out_path}")
    print(f"merged_records: {len(merged)}")

    if merged:
        print("first_merged_sample:")
        print(json.dumps(merged[0], ensure_ascii=False, indent=2))
    else:
        print("first_merged_sample: <none>")


if __name__ == "__main__":
    main()
