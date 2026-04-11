from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional

import torch

from dataset import collate_infer_batch, encode_units
from model import MacroEvalNet
from real_adapter import adapt_real_payload_to_sample, looks_like_real_payload


@dataclass
class OutcomePrediction:
    red_win_prob: float
    red_expected_survival: List[float]
    blue_expected_survival: List[float]
    red_expected_ammo_used: Optional[List[float]] = None
    blue_expected_ammo_used: Optional[List[float]] = None


class InferenceEngine:
    def __init__(self, model_path: Path, device: str = "cpu"):
        if device == "cuda" and not torch.cuda.is_available():
            raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")

        self.device = torch.device(device)
        self.model = MacroEvalNet().to(self.device)
        state = torch.load(model_path, map_location=self.device)
        self.model.load_state_dict(state, strict=False)
        self.model.eval()

    @staticmethod
    def _prepare_sample(payload: Dict[str, Any], input_format: str) -> Dict[str, Any]:
        if input_format == "real" or (input_format == "auto" and looks_like_real_payload(payload)):
            return adapt_real_payload_to_sample(payload)
        return payload

    @staticmethod
    def _uniform_survival(n: int, value: float) -> List[float]:
        if n <= 0:
            return []
        v = max(0.0, min(1.0, float(value)))
        return [v for _ in range(n)]

    def evaluate_tactic(self, payload: Dict[str, Any], input_format: str = "auto") -> Dict[str, Any]:
        sample = self._prepare_sample(payload, input_format)

        red_units = encode_units(sample["red_units"])
        blue_units = encode_units(sample["blue_units"])
        red_n = int(red_units.shape[0])
        blue_n = int(blue_units.shape[0])

        infer_item = {
            "red_units": red_units,
            "blue_units": blue_units,
            "tactic": torch.tensor(sample["tactic"], dtype=torch.float32),
        }
        batch = collate_infer_batch([infer_item])
        batch = {k: v.to(self.device) for k, v in batch.items()}

        with torch.no_grad():
            out = self.model(batch)

        red_win_prob = float(out["win_rate"].item())

        red_mask = batch["red_mask"][0]
        blue_mask = batch["blue_mask"][0]
        red_expected_survival = [float(v) for v in out["red_survival_probs"][0][red_mask][:red_n].tolist()]
        blue_expected_survival = [float(v) for v in out["blue_survival_probs"][0][blue_mask][:blue_n].tolist()]

        red_expected_ammo_used = [float(v) for v in out["red_missile_expenditure"][0][red_mask][:red_n].tolist()]
        blue_expected_ammo_used = [float(v) for v in out["blue_missile_expenditure"][0][blue_mask][:blue_n].tolist()]

        outcome = OutcomePrediction(
            red_win_prob=red_win_prob,
            red_expected_survival=red_expected_survival,
            blue_expected_survival=blue_expected_survival,
            red_expected_ammo_used=red_expected_ammo_used,
            blue_expected_ammo_used=blue_expected_ammo_used,
        )
        return asdict(outcome)

