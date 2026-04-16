import json
from datetime import datetime
from pathlib import Path
from typing import Any, Dict


def _pick_strict(aggregate: Dict[str, Any]) -> Dict[str, Any]:
    keys = [
        "trajectory_alive_accuracy",
        "terminal_win_accuracy",
        "critical_event_accuracy",
        "termination_flag_accuracy",
        "termination_flag_high_conf_hit_rate",
        "event_first_fire_accuracy",
        "event_fire_count_mae",
        "reward_red_mae",
        "reward_total_mae",
        "event_first_kill_accuracy",
        "event_objective_accuracy",
        "event_termination_accuracy",
    ]
    return {key: aggregate.get(key) for key in keys}


def _pick_cf(payload: Dict[str, Any]) -> Dict[str, Any]:
    metrics = payload.get("metrics", {}) or {}
    focus = payload.get("focus_metrics", {}) or {}

    def metric(name: str) -> Dict[str, Any]:
        return focus.get(name) or metrics.get(name, {})

    return {
        "valid": payload.get("valid"),
        "num_counterfactual_groups": payload.get("num_counterfactual_groups"),
        "num_counterfactual_pairs": payload.get("num_counterfactual_pairs"),
        "event_red_fire_count": {
            "delta_mae": metric("event_red_fire_count").get("delta_mae"),
            "num_signal_pairs": metric("event_red_fire_count").get("num_signal_pairs"),
            "sign_accuracy_on_signal_pairs": metric("event_red_fire_count").get("sign_accuracy_on_signal_pairs"),
        },
        "termination_flag": {
            "delta_mae": metric("termination_flag").get("delta_mae"),
            "num_signal_pairs": metric("termination_flag").get("num_signal_pairs"),
            "sign_accuracy_on_signal_pairs": metric("termination_flag").get("sign_accuracy_on_signal_pairs"),
        },
        "reward_red": {
            "delta_mae": metric("reward_red").get("delta_mae"),
            "num_signal_pairs": metric("reward_red").get("num_signal_pairs"),
            "sign_accuracy_on_signal_pairs": metric("reward_red").get("sign_accuracy_on_signal_pairs"),
        },
        "terminal_red_win_prob": {
            "delta_mae": metric("terminal_red_win_prob").get("delta_mae"),
            "num_signal_pairs": metric("terminal_red_win_prob").get("num_signal_pairs"),
            "sign_accuracy_on_signal_pairs": metric("terminal_red_win_prob").get("sign_accuracy_on_signal_pairs"),
        },
    }


def _delta(a: Any, b: Any, higher_better: bool) -> Any:
    if a is None or b is None:
        return None
    return (b - a) if higher_better else (a - b)


def main() -> None:
    root = Path(__file__).resolve().parent.parent / "data_world_model_cf"

    strict_105 = json.loads((root / "phase10_5_v106_strict_eval.json").read_text(encoding="utf-8"))["heldout_cf"]["aggregate"]
    strict_106 = json.loads((root / "phase10_6_final_strict_eval.json").read_text(encoding="utf-8"))["heldout_cf"]["aggregate"]
    cf_105 = json.loads((root / "phase10_5_v106_counterfactual_eval.json").read_text(encoding="utf-8"))
    cf_106 = json.loads((root / "phase10_6_final_counterfactual_eval.json").read_text(encoding="utf-8"))

    s105 = _pick_strict(strict_105)
    s106 = _pick_strict(strict_106)
    c105 = _pick_cf(cf_105)
    c106 = _pick_cf(cf_106)

    report = {
        "timestamp": datetime.now().isoformat(),
        "comparison": {
            "model_A": {
                "name": "Phase10.5 retrain baseline (re-evaluated under effective termination semantics)",
                "checkpoint": "..\\data_world_model_cf\\ab_phase10_5_retrain\\model.pt",
            },
            "model_B": {
                "name": "Phase10.6 targeted fix final",
                "checkpoint": "..\\data_world_model_cf\\phase10_6_ft_nocf\\model.pt",
            },
            "heldout_pack": "..\\data_world_model_cf\\heldout_pack",
            "shared_eval_config": {
                "splits": "heldout_cf",
                "horizons": ["20", "terminal"],
                "state_step": 0,
                "min_groups": 8,
                "min_pairs": 100,
            },
        },
        "strict_eval": {"phase10_5": s105, "phase10_6": s106},
        "counterfactual_eval": {"phase10_5": c105, "phase10_6": c106},
        "deltas": {
            "trajectory_alive_accuracy_gain": _delta(
                s105["trajectory_alive_accuracy"], s106["trajectory_alive_accuracy"], True
            ),
            "terminal_win_accuracy_gain": _delta(s105["terminal_win_accuracy"], s106["terminal_win_accuracy"], True),
            "critical_event_accuracy_gain": _delta(
                s105["critical_event_accuracy"], s106["critical_event_accuracy"], True
            ),
            "event_fire_count_mae_improvement": _delta(s105["event_fire_count_mae"], s106["event_fire_count_mae"], False),
            "reward_red_mae_improvement": _delta(s105["reward_red_mae"], s106["reward_red_mae"], False),
            "reward_total_mae_improvement": _delta(s105["reward_total_mae"], s106["reward_total_mae"], False),
            "cf_termination_signal_pairs_delta": _delta(
                c105["termination_flag"]["num_signal_pairs"], c106["termination_flag"]["num_signal_pairs"], True
            ),
            "cf_termination_sign_acc_gain": _delta(
                c105["termination_flag"]["sign_accuracy_on_signal_pairs"],
                c106["termination_flag"]["sign_accuracy_on_signal_pairs"],
                True,
            ),
            "cf_terminal_win_sign_acc_gain": _delta(
                c105["terminal_red_win_prob"]["sign_accuracy_on_signal_pairs"],
                c106["terminal_red_win_prob"]["sign_accuracy_on_signal_pairs"],
                True,
            ),
        },
    }

    json_path = root / "phase10_6_ab_report.json"
    md_path = root / "phase10_6_ab_report.md"
    json_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Phase10.5 vs Phase10.6 A/B Report",
        "",
        "## Strict Eval (heldout_cf)",
    ]
    for key in [
        "trajectory_alive_accuracy",
        "terminal_win_accuracy",
        "critical_event_accuracy",
        "termination_flag_accuracy",
        "termination_flag_high_conf_hit_rate",
        "event_first_fire_accuracy",
        "event_fire_count_mae",
        "reward_red_mae",
        "reward_total_mae",
    ]:
        lines.append(f"- {key}: phase10.5={s105.get(key)} | phase10.6={s106.get(key)}")

    lines.extend([
        "",
        "## Held-out Counterfactual Eval (heldout_cf)",
        f"- valid: phase10.5={c105['valid']} | phase10.6={c106['valid']}",
        f"- groups/pairs: phase10.5={c105['num_counterfactual_groups']}/{c105['num_counterfactual_pairs']} | phase10.6={c106['num_counterfactual_groups']}/{c106['num_counterfactual_pairs']}",
    ])

    for key in ["event_red_fire_count", "termination_flag", "reward_red", "terminal_red_win_prob"]:
        a = c105[key]
        b = c106[key]
        lines.append(
            f"- {key}: delta_mae {a['delta_mae']} -> {b['delta_mae']}; signal_pairs {a['num_signal_pairs']} -> {b['num_signal_pairs']}; sign_acc {a['sign_accuracy_on_signal_pairs']} -> {b['sign_accuracy_on_signal_pairs']}"
        )

    md_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {json_path}")
    print(f"Wrote {md_path}")


if __name__ == "__main__":
    main()
