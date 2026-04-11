import argparse
import random
from pathlib import Path
from typing import Dict

import numpy as np
import torch
import torch.nn.functional as F
from torch.utils.data import DataLoader

from data_io import read_json, read_jsonl, write_json
from dataset import EpisodeDataset, collate_episode
from model import MacroEvalNet


def set_seed(seed: int) -> None:
    random.seed(seed)
    np.random.seed(seed)
    torch.manual_seed(seed)


def move_to_device(batch: Dict[str, torch.Tensor], device: torch.device) -> Dict[str, torch.Tensor]:
    return {k: v.to(device) for k, v in batch.items()}


def evaluate(model: MacroEvalNet, loader: DataLoader, device: torch.device) -> Dict[str, float]:
    model.eval()
    total = 0
    win_correct = 0
    loss_mse = 0.0

    with torch.no_grad():
        for batch in loader:
            batch = move_to_device(batch, device)
            out = model(batch)
            win_prob = out["win_rate"]
            win_pred = (win_prob >= 0.5).float()

            win_correct += (win_pred == batch["red_win"]).sum().item()
            red_valid = batch["red_mask"].float()
            blue_valid = batch["blue_mask"].float()
            red_alive_err = ((out["red_survival_probs"] - batch["red_final_alive"]) ** 2 * red_valid).sum()
            blue_alive_err = ((out["blue_survival_probs"] - batch["blue_final_alive"]) ** 2 * blue_valid).sum()

            red_missile_err = ((out["red_final_missile"] - batch["red_final_missile"]) ** 2 * red_valid).sum()
            blue_missile_err = ((out["blue_final_missile"] - batch["blue_final_missile"]) ** 2 * blue_valid).sum()

            mse = red_alive_err + blue_alive_err + red_missile_err + blue_missile_err
            loss_mse += mse.item()
            total += batch["red_win"].shape[0]

    return {
        "accuracy": win_correct / max(total, 1),
        "loss_mse": loss_mse / max(total, 1),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Train MVP macro evaluation model.")
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--epochs", type=int, default=20)
    parser.add_argument("--batch-size", type=int, default=64)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()

    set_seed(args.seed)
    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    train_ds = EpisodeDataset(read_jsonl(args.data_dir / "processed" / "train.jsonl"))
    val_ds = EpisodeDataset(read_jsonl(args.data_dir / "processed" / "val.jsonl"))

    train_loader = DataLoader(train_ds, batch_size=args.batch_size, shuffle=True, collate_fn=collate_episode)
    val_loader = DataLoader(val_ds, batch_size=args.batch_size, shuffle=False, collate_fn=collate_episode)

    model = MacroEvalNet().to(device)
    opt = torch.optim.Adam(model.parameters(), lr=args.lr)

    best_acc = -1.0
    history = []

    for epoch in range(1, args.epochs + 1):
        model.train()
        train_loss_sum = 0.0
        train_count = 0
        for batch in train_loader:
            batch = move_to_device(batch, device)
            out = model(batch)

            win_loss = F.binary_cross_entropy(out["win_rate"], batch["red_win"])

            red_valid = batch["red_mask"].float()
            blue_valid = batch["blue_mask"].float()

            red_alive_loss = (F.binary_cross_entropy(out["red_survival_probs"], batch["red_final_alive"], reduction="none") * red_valid).sum() / red_valid.sum().clamp(min=1.0)
            blue_alive_loss = (F.binary_cross_entropy(out["blue_survival_probs"], batch["blue_final_alive"], reduction="none") * blue_valid).sum() / blue_valid.sum().clamp(min=1.0)

            red_missile_loss = ((out["red_final_missile"] - batch["red_final_missile"]) ** 2 * red_valid).sum() / red_valid.sum().clamp(min=1.0)
            blue_missile_loss = ((out["blue_final_missile"] - batch["blue_final_missile"]) ** 2 * blue_valid).sum() / blue_valid.sum().clamp(min=1.0)

            loss = win_loss + 0.8 * (red_alive_loss + blue_alive_loss) + 0.3 * (red_missile_loss + blue_missile_loss)

            opt.zero_grad()
            loss.backward()
            opt.step()

            bsz = batch["red_win"].shape[0]
            train_loss_sum += loss.item() * bsz
            train_count += bsz

        metrics = evaluate(model, val_loader, device)
        avg_train_loss = train_loss_sum / max(train_count, 1)
        history.append({"epoch": epoch, "train_loss": avg_train_loss, **metrics})
        print(
            f"Epoch {epoch:03d} | train_loss={avg_train_loss:.4f} | "
            f"val_acc={metrics['accuracy']:.4f} | val_mse={metrics['loss_mse']:.4f}"
        )

        if metrics["accuracy"] > best_acc:
            best_acc = metrics["accuracy"]
            args.out_dir.mkdir(parents=True, exist_ok=True)
            torch.save(model.state_dict(), args.out_dir / "model.pt")

    write_json(args.out_dir / "train_history.json", {"history": history})

    processed_summary = read_json(args.data_dir / "processed" / "summary.json")
    write_json(
        args.out_dir / "meta.json",
        {
            "seed": args.seed,
            "epochs": args.epochs,
            "batch_size": args.batch_size,
            "lr": args.lr,
            "processed_summary": processed_summary,
        },
    )
    print("Training finished. Best val accuracy:", best_acc)


if __name__ == "__main__":
    main()


