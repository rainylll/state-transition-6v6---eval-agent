import json
from datetime import datetime
from pathlib import Path
from typing import Any, Dict


def _pick_strict(metrics: Dict[str, Any]) -> Dict[str, Any]:
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
        "reward_red_delta_scale",
        "reward_red_true_delta_scale",
    ]
    return {key: metrics.get(key) for key in keys}


def _pick_cf(metrics: Dict[str, Any]) -> Dict[str, Any]:
    output: Dict[str, Any] = {
        "valid": metrics.get("valid"),
        "coverage_insufficient": metrics.get("coverage_insufficient"),
        "num_counterfactual_groups": metrics.get("num_counterfactual_groups"),
        "num_counterfactual_pairs": metrics.get("num_counterfactual_pairs"),
    }
    focus: Dict[str, Any] = {}
    for key in ["event_red_fire_count", "termination_flag", "reward_red"]:
        entry = (metrics.get("focus_metrics", {}) or {}).get(key, {})
        focus[key] = {
            "delta_mae": entry.get("delta_mae"),
            "sign_accuracy_on_signal_pairs": entry.get("sign_accuracy_on_signal_pairs"),
            "num_signal_pairs": entry.get("num_signal_pairs"),
        }

    terminal_win = (metrics.get("metrics", {}) or {}).get("terminal_red_win_prob", {})
    focus["terminal_red_win_prob"] = {
        "delta_mae": terminal_win.get("delta_mae"),
        "sign_accuracy_on_signal_pairs": terminal_win.get("sign_accuracy_on_signal_pairs"),
        "num_signal_pairs": terminal_win.get("num_signal_pairs"),
    }
    output["focus_metrics"] = focus
    return output


def _delta(a: Any, b: Any, higher_better: bool = True) -> Any:
    if a is None or b is None:
        return None
    return (b - a) if higher_better else (a - b)


def main() -> None:
    base_dir = Path(__file__).resolve().parent.parent / "data_world_model_cf"

    strict_a = json.loads((base_dir / "ab_phase7_strict_eval.json").read_text(encoding="utf-8"))["heldout_cf"]["aggregate"]
    strict_b = json.loads((base_dir / "ab_phase10_5_retrain_strict_eval.json").read_text(encoding="utf-8"))["heldout_cf"]["aggregate"]
    cf_a = json.loads((base_dir / "ab_phase7_counterfactual_eval_with_gate.json").read_text(encoding="utf-8"))
    cf_b = json.loads((base_dir / "ab_phase10_5_retrain_counterfactual_eval_with_gate.json").read_text(encoding="utf-8"))

    strict_a_p = _pick_strict(strict_a)
    strict_b_p = _pick_strict(strict_b)
    cf_a_p = _pick_cf(cf_a)
    cf_b_p = _pick_cf(cf_b)

    summary_deltas = {
        "trajectory_alive_accuracy_gain": _delta(
            strict_a_p["trajectory_alive_accuracy"], strict_b_p["trajectory_alive_accuracy"], True
        ),
        "terminal_win_accuracy_gain": _delta(
            strict_a_p["terminal_win_accuracy"], strict_b_p["terminal_win_accuracy"], True
        ),
        "critical_event_accuracy_gain": _delta(
            strict_a_p["critical_event_accuracy"], strict_b_p["critical_event_accuracy"], True
        ),
        "termination_flag_accuracy_gain": _delta(
            strict_a_p["termination_flag_accuracy"], strict_b_p["termination_flag_accuracy"], True
        ),
        "termination_flag_high_conf_hit_rate_gain": _delta(
            strict_a_p["termination_flag_high_conf_hit_rate"],
            strict_b_p["termination_flag_high_conf_hit_rate"],
            True,
        ),
        "event_fire_count_mae_improvement": _delta(
            strict_a_p["event_fire_count_mae"], strict_b_p["event_fire_count_mae"], False
        ),
        "reward_red_mae_improvement": _delta(strict_a_p["reward_red_mae"], strict_b_p["reward_red_mae"], False),
        "reward_total_mae_improvement": _delta(
            strict_a_p["reward_total_mae"], strict_b_p["reward_total_mae"], False
        ),
        "reward_delta_scale_gap_to_true_A": abs(
            (strict_a_p["reward_red_delta_scale"] or 0.0) - (strict_a_p["reward_red_true_delta_scale"] or 0.0)
        ),
        "reward_delta_scale_gap_to_true_B": abs(
            (strict_b_p["reward_red_delta_scale"] or 0.0) - (strict_b_p["reward_red_true_delta_scale"] or 0.0)
        ),
        "cf_event_red_fire_count_delta_mae_improvement": _delta(
            cf_a_p["focus_metrics"]["event_red_fire_count"]["delta_mae"],
            cf_b_p["focus_metrics"]["event_red_fire_count"]["delta_mae"],
            False,
        ),
        "cf_event_red_fire_count_sign_acc_gain": _delta(
            cf_a_p["focus_metrics"]["event_red_fire_count"]["sign_accuracy_on_signal_pairs"],
            cf_b_p["focus_metrics"]["event_red_fire_count"]["sign_accuracy_on_signal_pairs"],
            True,
        ),
        "cf_reward_red_delta_mae_improvement": _delta(
            cf_a_p["focus_metrics"]["reward_red"]["delta_mae"],
            cf_b_p["focus_metrics"]["reward_red"]["delta_mae"],
            False,
        ),
        "cf_reward_red_sign_acc_gain": _delta(
            cf_a_p["focus_metrics"]["reward_red"]["sign_accuracy_on_signal_pairs"],
            cf_b_p["focus_metrics"]["reward_red"]["sign_accuracy_on_signal_pairs"],
            True,
        ),
        "cf_termination_flag_delta_mae_improvement": _delta(
            cf_a_p["focus_metrics"]["termination_flag"]["delta_mae"],
            cf_b_p["focus_metrics"]["termination_flag"]["delta_mae"],
            False,
        ),
        "cf_terminal_win_delta_mae_improvement": _delta(
            cf_a_p["focus_metrics"]["terminal_red_win_prob"]["delta_mae"],
            cf_b_p["focus_metrics"]["terminal_red_win_prob"]["delta_mae"],
            False,
        ),
        "cf_terminal_win_sign_acc_gain": _delta(
            cf_a_p["focus_metrics"]["terminal_red_win_prob"]["sign_accuracy_on_signal_pairs"],
            cf_b_p["focus_metrics"]["terminal_red_win_prob"]["sign_accuracy_on_signal_pairs"],
            True,
        ),
    }

    report = {
        "timestamp": datetime.now().isoformat(),
        "ab_setup": {
            "model_A": {
                "name": "Phase7 functional baseline",
                "checkpoint": "..\\data_world_model\\phase7_functional_cf_run\\model.pt",
            },
            "model_B": {
                "name": "Phase10.5 retrained on train_visible_cf",
                "checkpoint": "..\\data_world_model_cf\\ab_phase10_5_retrain\\model.pt",
                "trained_now": True,
                "train_command": (
                    "train_world_model.py --data-dir ../data_world_model_cf/ab_train_visible_only "
                    "--counterfactual-data-dir ../data_world_model_cf/heldout_pack "
                    "--counterfactual-train-splits train_visible_cf --counterfactual-horizons 20,terminal "
                    "--counterfactual-state-step 0"
                ),
            },
            "heldout_pack": "..\\data_world_model_cf\\heldout_pack",
            "shared_eval_config": {
                "strict": {"splits": "heldout_cf"},
                "counterfactual": {
                    "splits": "heldout_cf",
                    "horizons": ["20", "terminal"],
                    "state_step": 0,
                    "min_groups": 8,
                    "min_pairs": 100,
                },
            },
        },
        "strict_eval": {"A": strict_a_p, "B": strict_b_p},
        "counterfactual_eval": {"A": cf_a_p, "B": cf_b_p},
        "summary_deltas": summary_deltas,
        "decision": {
            "worth_continue_phase10_5": True,
            "rationale": [
                "reward prediction error and reward delta-scale mismatch improved substantially on held-out strict eval",
                "counterfactual event and reward discrimination improved on key focus metrics",
                "critical_event_accuracy is still below baseline and needs targeted head-level fixing",
            ],
        },
    }

    json_out = base_dir / "ab_phase10_5_report.json"
    md_out = base_dir / "ab_phase10_5_report.md"
    json_out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    md_lines = [
        "# Phase 10.5 A/B 验证报告（Held-out Counterfactual）",
        "",
        "## 评估设置",
        "- Model A: Phase7 functional baseline",
        "- Model B: Phase10.5 retrain（train_visible_cf + CF finetune）",
        "- Held-out pack: ../data_world_model_cf/heldout_pack",
        "- Counterfactual gate: min_groups=8, min_pairs=100（A/B 均 valid）",
        "",
        "## Strict Eval（heldout_cf）",
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
        "reward_red_delta_scale",
        "reward_red_true_delta_scale",
    ]:
        md_lines.append(f"- {key}: A={strict_a_p.get(key)} | B={strict_b_p.get(key)}")

    md_lines.extend(
        [
            "",
            "## Held-out Counterfactual Eval（heldout_cf）",
            (
                f"- groups/pairs: A={cf_a_p['num_counterfactual_groups']}/"
                f"{cf_a_p['num_counterfactual_pairs']} | "
                f"B={cf_b_p['num_counterfactual_groups']}/{cf_b_p['num_counterfactual_pairs']}"
            ),
            f"- valid: A={cf_a_p['valid']} | B={cf_b_p['valid']}",
        ]
    )

    for key in ["event_red_fire_count", "termination_flag", "reward_red", "terminal_red_win_prob"]:
        metric_a = cf_a_p["focus_metrics"][key]
        metric_b = cf_b_p["focus_metrics"][key]
        md_lines.append(
            (
                f"- {key}: delta_mae A={metric_a['delta_mae']} | B={metric_b['delta_mae']}; "
                f"sign_acc A={metric_a['sign_accuracy_on_signal_pairs']} | "
                f"B={metric_b['sign_accuracy_on_signal_pairs']}; "
                f"signal_pairs={metric_b['num_signal_pairs']}"
            )
        )

    md_lines.extend(
        [
            "",
            "## 结论",
            "- Phase10.5 路线在 reward 与 counterfactual 区分能力上出现明确提升，且未出现 trajectory/terminal 的同步退化。",
            "- 仍存在 tradeoff：critical_event_accuracy 低于 baseline，需要对关键事件头做定向修补。",
            "- 建议继续沿 Phase10.5 深入，但下一步应聚焦关键事件命中与高置信校准。",
        ]
    )

    md_out.write_text("\n".join(md_lines), encoding="utf-8")
    print(f"Wrote {json_out}")
    print(f"Wrote {md_out}")


if __name__ == "__main__":
    main()
