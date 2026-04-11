from typing import Dict, List, Tuple

import torch
from torch.utils.data import Dataset

UNIT_TYPE_TO_ID = {"fighter": 0, "ship": 1}
UNIT_FEATURES = ("speed", "sensor", "weapon", "defense", "hp", "ecm")
UNIT_FEATURES_V2 = ("speed", "sensor_range", "missile", "alive", "relative_distance")


def encode_units(units: List[Dict]) -> torch.Tensor:
    rows = []
    for u in units:
        type_id = float(u.get("type_id", UNIT_TYPE_TO_ID.get(u.get("type", "fighter"), 0)))
        if "features" in u:
            feat = [float(x) for x in u["features"]]
            if len(feat) == len(UNIT_FEATURES_V2):
                rows.append([type_id, *feat])
                continue
            if len(feat) == len(UNIT_FEATURES_V2) + 1:
                rows.append(feat)
                continue
            if len(feat) == 7:
                # [type_id, speed, sensor, missile, lon, lat, alive] -> model payload with temporary zero distance.
                rows.append([feat[0], feat[1], feat[2], feat[3], feat[6], 0.0])
                continue
            raise ValueError(f"Expected 5D/6D/7D features, got {len(feat)}")

        if all(k in u for k in UNIT_FEATURES_V2):
            rows.append([type_id, *[float(u[k]) for k in UNIT_FEATURES_V2]])
            continue

        # Backward compatibility for legacy unit schema with string type.
        type_bias = float(UNIT_TYPE_TO_ID.get(u.get("type", "fighter"), 0))
        legacy = [float(u[k]) for k in UNIT_FEATURES]
        rows.append([
            type_bias,
            legacy[0],
            legacy[1],
            legacy[2],
            legacy[4],
            0.0,
        ])

    if not rows:
        return torch.zeros((0, len(UNIT_FEATURES_V2) + 1), dtype=torch.float32)
    return torch.tensor(rows, dtype=torch.float32)


def _entity_targets(r: Dict, side: str, encoded_units: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
    labels = r.get("labels", {})
    n = int(encoded_units.shape[0])
    loss_key = "red_loss" if side == "red" else "blue_loss"

    alive_vals = labels.get(f"{side}_final_alive")
    if not isinstance(alive_vals, list) or len(alive_vals) != n:
        base_alive = 1.0 - float(labels.get(loss_key, 0.5))
        alive_vals = [base_alive for _ in range(n)]

    missile_vals = labels.get(f"{side}_final_missile")
    if not isinstance(missile_vals, list) or len(missile_vals) != n:
        if n > 0:
            initial_missile = encoded_units[:, 3].tolist()
            missile_vals = [float(m) * float(a) for m, a in zip(initial_missile, alive_vals)]
        else:
            missile_vals = []

    return (
        torch.tensor([float(x) for x in alive_vals], dtype=torch.float32),
        torch.tensor([max(0.0, float(x)) for x in missile_vals], dtype=torch.float32),
    )


class EpisodeDataset(Dataset):
    def __init__(self, records: List[Dict]):
        self.records = records

    def __len__(self) -> int:
        return len(self.records)

    def __getitem__(self, idx: int) -> Dict:
        r = self.records[idx]
        labels = r["labels"]
        red_units = encode_units(r["red_units"])
        blue_units = encode_units(r["blue_units"])
        red_alive, red_missile = _entity_targets(r, "red", red_units)
        blue_alive, blue_missile = _entity_targets(r, "blue", blue_units)
        return {
            "red_units": red_units,
            "blue_units": blue_units,
            "tactic": torch.tensor(r["tactic"], dtype=torch.float32),
            "red_win": torch.tensor(labels["red_win"], dtype=torch.float32),
            "red_loss": torch.tensor(labels["red_loss"], dtype=torch.float32),
            "blue_loss": torch.tensor(labels["blue_loss"], dtype=torch.float32),
            "red_final_alive": red_alive,
            "blue_final_alive": blue_alive,
            "red_final_missile": red_missile,
            "blue_final_missile": blue_missile,
        }


def _pad_side(batch_units: List[torch.Tensor]) -> Tuple[torch.Tensor, torch.Tensor]:
    max_len = max(max(t.shape[0], 1) for t in batch_units)
    feat_dim = batch_units[0].shape[1] if batch_units[0].ndim == 2 else len(UNIT_FEATURES_V2)
    padded = torch.zeros((len(batch_units), max_len, feat_dim), dtype=torch.float32)
    mask = torch.zeros((len(batch_units), max_len), dtype=torch.bool)
    for i, t in enumerate(batch_units):
        n = t.shape[0]
        if n > 0:
            padded[i, :n, :] = t
            mask[i, :n] = True
    return padded, mask


def _pad_targets(batch_targets: List[torch.Tensor], max_len: int) -> torch.Tensor:
    padded = torch.zeros((len(batch_targets), max_len), dtype=torch.float32)
    for i, t in enumerate(batch_targets):
        n = min(int(t.shape[0]), max_len)
        if n > 0:
            padded[i, :n] = t[:n]
    return padded


def collate_episode(batch: List[Dict]) -> Dict:
    red_units, red_mask = _pad_side([x["red_units"] for x in batch])
    blue_units, blue_mask = _pad_side([x["blue_units"] for x in batch])

    red_final_alive = _pad_targets([x["red_final_alive"] for x in batch], red_units.shape[1])
    blue_final_alive = _pad_targets([x["blue_final_alive"] for x in batch], blue_units.shape[1])
    red_final_missile = _pad_targets([x["red_final_missile"] for x in batch], red_units.shape[1])
    blue_final_missile = _pad_targets([x["blue_final_missile"] for x in batch], blue_units.shape[1])

    return {
        "red_units": red_units,
        "red_mask": red_mask,
        "blue_units": blue_units,
        "blue_mask": blue_mask,
        "tactic": torch.stack([x["tactic"] for x in batch], dim=0),
        "red_win": torch.stack([x["red_win"] for x in batch], dim=0),
        "red_loss": torch.stack([x["red_loss"] for x in batch], dim=0),
        "blue_loss": torch.stack([x["blue_loss"] for x in batch], dim=0),
        "red_final_alive": red_final_alive,
        "blue_final_alive": blue_final_alive,
        "red_final_missile": red_final_missile,
        "blue_final_missile": blue_final_missile,
    }


def collate_infer_batch(batch: List[Dict]) -> Dict:
    red_units, red_mask = _pad_side([x["red_units"] for x in batch])
    blue_units, blue_mask = _pad_side([x["blue_units"] for x in batch])
    return {
        "red_units": red_units,
        "red_mask": red_mask,
        "blue_units": blue_units,
        "blue_mask": blue_mask,
        "tactic": torch.stack([x["tactic"] for x in batch], dim=0),
    }


