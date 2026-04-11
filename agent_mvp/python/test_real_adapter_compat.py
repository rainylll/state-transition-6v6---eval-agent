import json
from pathlib import Path

import torch

from dataset import collate_infer_batch, encode_units
from model import MacroEvalNet
from real_adapter import adapt_real_payload_to_sample


def _load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _to_infer_item(sample):
    return {
        "red_units": encode_units(sample["red_units"]),
        "blue_units": encode_units(sample["blue_units"]),
        "tactic": torch.tensor(sample["tactic"], dtype=torch.float32),
    }


def main() -> None:
    repo = Path(__file__).resolve().parents[2]
    raw = _load_json(repo / "agent_mvp" / "examples" / "mock_real_case.json")

    sample_a = adapt_real_payload_to_sample(raw)

    # Make a shorter second sample to verify dynamic padding + key padding mask alignment.
    raw_short = _load_json(repo / "agent_mvp" / "examples" / "mock_real_case.json")
    raw_short["blue"]["ships"] = raw_short["blue"]["ships"][:1]
    raw_short["red"]["aircraft"] = raw_short["red"]["aircraft"][:1]
    sample_b = adapt_real_payload_to_sample(raw_short)

    batch = collate_infer_batch([_to_infer_item(sample_a), _to_infer_item(sample_b)])

    print("red_units shape:", tuple(batch["red_units"].shape))
    print("blue_units shape:", tuple(batch["blue_units"].shape))
    print("red_mask shape:", tuple(batch["red_mask"].shape), "valid:", batch["red_mask"].sum(dim=1).tolist())
    print("blue_mask shape:", tuple(batch["blue_mask"].shape), "valid:", batch["blue_mask"].sum(dim=1).tolist())
    print("tactic shape:", tuple(batch["tactic"].shape))

    model = MacroEvalNet()
    model.train()
    out = model(batch)

    # Dummy objective only for backward compatibility check.
    loss = (
        out["win_rate"].pow(2).mean()
        + out["red_survival_probs"].mean()
        + out["blue_survival_probs"].mean()
        + out["red_missile_expenditure"].mean()
        + out["blue_missile_expenditure"].mean()
    )
    loss.backward()

    print("forward/backward ok, loss:", float(loss.item()))


if __name__ == "__main__":
    main()

