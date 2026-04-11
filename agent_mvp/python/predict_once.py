import argparse
from pathlib import Path

from data_io import read_json, write_json
from inference_api import InferenceEngine


def main() -> None:
    parser = argparse.ArgumentParser(description="Run one-shot macro evaluation inference.")
    parser.add_argument("--model-path", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--input-format", type=str, default="auto", choices=["auto", "sample", "real"])
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    args = parser.parse_args()

    payload = read_json(args.input)

    engine = InferenceEngine(model_path=args.model_path, device=args.device)
    outcome = engine.evaluate_tactic(payload, input_format=args.input_format)

    # Keep old keys so existing callers are not broken.
    result = {
        "p_red_win": outcome["red_win_prob"],
        "red_loss_est": 1.0 - (sum(outcome["red_expected_survival"]) / max(len(outcome["red_expected_survival"]), 1)),
        "blue_loss_est": 1.0 - (sum(outcome["blue_expected_survival"]) / max(len(outcome["blue_expected_survival"]), 1)),
        **outcome,
    }

    write_json(args.output, result)
    print(result)


if __name__ == "__main__":
    main()


