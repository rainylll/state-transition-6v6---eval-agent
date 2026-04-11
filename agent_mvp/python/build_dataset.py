import argparse
import random
from pathlib import Path

import yaml

from data_io import read_jsonl, write_json, write_jsonl
from simulator_stub import sample_episode


def main() -> None:
    parser = argparse.ArgumentParser(description="Build MVP macro-eval dataset.")
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--episodes", type=int, default=5000)
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()

    with args.config.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f)

    rng = random.Random(args.seed)
    episodes = [sample_episode(rng, cfg) for _ in range(args.episodes)]

    split_cfg = cfg.get("split", {"train": 0.7, "val": 0.15, "test": 0.15})
    rng.shuffle(episodes)

    n = len(episodes)
    n_train = int(n * split_cfg["train"])
    n_val = int(n * split_cfg["val"])

    train_records = episodes[:n_train]
    val_records = episodes[n_train : n_train + n_val]
    test_records = episodes[n_train + n_val :]

    write_jsonl(args.out_dir / "raw" / "episodes.jsonl", episodes)
    write_jsonl(args.out_dir / "processed" / "train.jsonl", train_records)
    write_jsonl(args.out_dir / "processed" / "val.jsonl", val_records)
    write_jsonl(args.out_dir / "processed" / "test.jsonl", test_records)

    summary = {
        "episodes": n,
        "train": len(train_records),
        "val": len(val_records),
        "test": len(test_records),
        "seed": args.seed,
    }
    write_json(args.out_dir / "processed" / "summary.json", summary)

    # quick sanity check
    _ = read_jsonl(args.out_dir / "processed" / "train.jsonl")
    print("Dataset ready:", summary)


if __name__ == "__main__":
    main()

