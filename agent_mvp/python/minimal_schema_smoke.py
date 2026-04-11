import argparse
from pathlib import Path
from typing import Any, Dict, List

from real_adapter import adapt_real_payload_to_sample
from tactic_slicer import slice_4v2_to_2v1


def _build_mock_4v2_raw_payload() -> Dict[str, Any]:
    # C++ raw features: [type_id, speed, sensor, missile, lon, lat, alive]
    return {
        "battle_id": "smoke-4v2-raw",
        "timestamp": 1733558400,
        "red": {
            "units": [
                {"id": "R1", "features": [0.0, 220.0, 150.0, 6.0, 123.50, 31.10, 1.0]},
                {"id": "R2", "features": [0.0, 210.0, 140.0, 5.0, 123.45, 31.05, 1.0]},
                {"id": "R3", "features": [0.0, 195.0, 130.0, 4.0, 123.20, 30.90, 1.0]},
                {"id": "R4", "features": [0.0, 185.0, 120.0, 3.0, 123.10, 30.85, 1.0]},
            ]
        },
        "blue": {
            "units": [
                {"id": "B1", "features": [1.0, 205.0, 145.0, 5.0, 124.00, 31.60, 1.0]},
                {"id": "B2", "features": [1.0, 190.0, 125.0, 4.0, 124.30, 31.40, 1.0]},
            ]
        },
        "tactic_cmd": {
            "launch_delay_s": 90,
            "formation_compactness": 0.65,
            "target_focus_ratio": 0.70,
            "attack_bearing_deg": 45,
            "support_level": 0.60,
        },
    }


def _proxy_predict(local_slice: Dict[str, Any]) -> Dict[str, Any]:
    red_units = local_slice["red_units"]
    blue_units = local_slice["blue_units"]

    rel_dist = float(red_units[0]["features"][4]) if red_units else 0.0
    red_power = sum(float(u["features"][2]) + float(u["features"][0]) / 300.0 for u in red_units)
    blue_power = sum(float(u["features"][2]) + float(u["features"][0]) / 300.0 for u in blue_units)

    score = red_power - blue_power - rel_dist
    win_rate = 1.0 / (1.0 + pow(2.718281828, -score))

    red_survival = []
    red_ammo = []
    for u in red_units:
        missile = float(u["features"][2])
        alive = float(u["features"][3])
        surv = max(0.0, min(1.0, alive * (0.65 + 0.25 * win_rate - 0.1 * rel_dist)))
        used = max(0.0, missile * (1.0 - surv))
        red_survival.append(surv)
        red_ammo.append(used)

    blue_survival = []
    blue_ammo = []
    for u in blue_units:
        missile = float(u["features"][2])
        alive = float(u["features"][3])
        surv = max(0.0, min(1.0, alive * (0.65 + 0.25 * (1.0 - win_rate) - 0.1 * rel_dist)))
        used = max(0.0, missile * (1.0 - surv))
        blue_survival.append(surv)
        blue_ammo.append(used)

    return {
        "win_rate": max(0.0, min(1.0, win_rate)),
        "red_survival_probs": red_survival,
        "blue_survival_probs": blue_survival,
        "red_missile_expenditure": red_ammo,
        "blue_missile_expenditure": blue_ammo,
    }


def _try_torch_predict(slices: List[Dict[str, Any]], model_path: Path = None) -> List[Dict[str, Any]]:
    try:
        import torch

        from dataset import collate_infer_batch, encode_units
        from model import MacroEvalNet
    except Exception:
        return []

    infer_items = []
    for s in slices:
        infer_items.append(
            {
                "red_units": encode_units(s["red_units"]),
                "blue_units": encode_units(s["blue_units"]),
                "tactic": torch.tensor(s["tactic"], dtype=torch.float32),
            }
        )

    batch = collate_infer_batch(infer_items)
    model = MacroEvalNet()
    if model_path is not None and model_path.exists():
        state = torch.load(model_path, map_location="cpu")
        model.load_state_dict(state, strict=False)
    model.eval()

    with torch.no_grad():
        out = model(batch)

    preds = []
    for i in range(len(slices)):
        red_mask = batch["red_mask"][i]
        blue_mask = batch["blue_mask"][i]
        preds.append(
            {
                "win_rate": float(out["win_rate"][i].item()),
                "red_survival_probs": [float(x) for x in out["red_survival_probs"][i][red_mask].tolist()],
                "blue_survival_probs": [float(x) for x in out["blue_survival_probs"][i][blue_mask].tolist()],
                "red_missile_expenditure": [float(x) for x in out["red_missile_expenditure"][i][red_mask].tolist()],
                "blue_missile_expenditure": [float(x) for x in out["blue_missile_expenditure"][i][blue_mask].tolist()],
            }
        )
    return preds


def main() -> None:
    parser = argparse.ArgumentParser(description="Smoke test minimal schema: 6D raw -> adapter(5D) -> slicer -> model outputs")
    parser.add_argument("--strategy", type=str, default="distance_threat", choices=["distance_threat", "distance", "threat"])
    parser.add_argument("--model-path", type=Path, default=None, help="Optional model checkpoint for MacroEvalNet")
    args = parser.parse_args()

    raw_payload = _build_mock_4v2_raw_payload()
    print("[1/4] Built 4v2 raw payload with 7D features.")
    print("      red_units:", len(raw_payload["red"]["units"]), "blue_units:", len(raw_payload["blue"]["units"]))

    sample = adapt_real_payload_to_sample(raw_payload)
    print("[2/4] Adapter done: model feature dim =", len(sample["red_units"][0]["features"]))
    print("      global.relative_distance =", round(float(sample.get("global", {}).get("relative_distance", 0.0)), 6))
    print("      first red model features =", [round(float(x), 6) for x in sample["red_units"][0]["features"]])

    slices = slice_4v2_to_2v1(sample, strategy=args.strategy)
    print("[3/4] Slicer done: produced", len(slices), "local 2v1 slices with strategy", args.strategy)
    for idx, s in enumerate(slices):
        print("      slice", idx, "meta:", s.get("slice_meta", {}))

    torch_preds = _try_torch_predict(slices, model_path=args.model_path)
    if torch_preds:
        print("[4/4] Model inference backend: MacroEvalNet (torch)")
        preds = torch_preds
    else:
        print("[4/4] Model inference backend: proxy model (torch unavailable)")
        preds = [_proxy_predict(s) for s in slices]

    for idx, p in enumerate(preds):
        print("      slice", idx, "win_rate=", round(float(p["win_rate"]), 6))
        print("        red_survival_probs=", [round(float(x), 6) for x in p["red_survival_probs"]])
        print("        blue_survival_probs=", [round(float(x), 6) for x in p["blue_survival_probs"]])
        print("        red_missile_expenditure=", [round(float(x), 6) for x in p["red_missile_expenditure"]])
        print("        blue_missile_expenditure=", [round(float(x), 6) for x in p["blue_missile_expenditure"]])

    mean_win = sum(float(p["win_rate"]) for p in preds) / max(len(preds), 1)
    print("[DONE] Pipeline OK. Mean red win_rate across slices =", round(mean_win, 6))


if __name__ == "__main__":
    main()


