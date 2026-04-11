import subprocess
import sys
from pathlib import Path


def run(cmd):
    print("[RUN]", " ".join(cmd))
    subprocess.check_call(cmd)


def main() -> None:
    repo = Path(__file__).resolve().parents[2]
    mvp = repo / "agent_mvp"
    py = mvp / "python"
    data_dir = mvp / "data"
    art_dir = mvp / "artifacts"

    run(
        [
            sys.executable,
            str(py / "build_dataset.py"),
            "--config",
            str(mvp / "configs" / "scenario_grid.yaml"),
            "--out-dir",
            str(data_dir),
            "--episodes",
            "300",
            "--seed",
            "11",
        ]
    )

    run(
        [
            sys.executable,
            str(py / "train.py"),
            "--data-dir",
            str(data_dir),
            "--out-dir",
            str(art_dir),
            "--epochs",
            "2",
            "--batch-size",
            "64",
            "--device",
            "cpu",
        ]
    )

    run(
        [
            sys.executable,
            str(py / "eval.py"),
            "--data-dir",
            str(data_dir),
            "--model-path",
            str(art_dir / "model.pt"),
            "--out-path",
            str(art_dir / "metrics.json"),
            "--device",
            "cpu",
        ]
    )

    run(
        [
            sys.executable,
            str(py / "predict_once.py"),
            "--model-path",
            str(art_dir / "model.pt"),
            "--input",
            str(mvp / "examples" / "sample_case.json"),
            "--output",
            str(art_dir / "prediction.json"),
            "--device",
            "cpu",
        ]
    )

    print("Smoke test completed.")


if __name__ == "__main__":
    main()


