import argparse
from pathlib import Path

import torch
from torch.utils.data import DataLoader

from data_io import read_jsonl, write_json
from dataset import EpisodeDataset, collate_episode
from model import MacroEvalNet
from train import evaluate


def main() -> None:
    parser = argparse.ArgumentParser(description="Evaluate MVP macro evaluation model.")
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--out-path", type=Path, required=True)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    args = parser.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    test_ds = EpisodeDataset(read_jsonl(args.data_dir / "processed" / "test.jsonl"))
    test_loader = DataLoader(test_ds, batch_size=args.batch_size, shuffle=False, collate_fn=collate_episode)

    model = MacroEvalNet().to(device)
    model.load_state_dict(torch.load(args.model_path, map_location=device))

    metrics = evaluate(model, test_loader, device)
    write_json(args.out_path, metrics)
    print("Test metrics:", metrics)


if __name__ == "__main__":
    main()


