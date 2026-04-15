import argparse
import math
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

import torch

from data_io import read_jsonl
from trajectory_to_acmi import (
    TrajectoryEpisode,
    TrajectoryEvent,
    TrajectoryFrame,
    TrajectoryObjectState,
    write_acmi,
)
from world_model import WorldModelNet
from world_model_dataset import (
    EVENT_COUNT_KEYS,
    EVENT_FLAG_KEYS,
    FAMILY_TO_ID,
    HORIZON_TO_ID,
    SOURCE_TO_ID,
)

MISSILE_SCALE = 8.0
LON_SCALE_DEG = 180.0
LAT_SCALE_DEG = 90.0
ALT_SCALE_M = 20000.0
SPEED_SCALE_MPS = 1000.0

FIRST_EVENT_TEXT = {
    "red_first_contact_flag": ("Bookmark", "Red first contact"),
    "blue_first_contact_flag": ("Bookmark", "Blue first contact"),
    "red_first_fire_flag": ("Bookmark", "Red first fire"),
    "blue_first_fire_flag": ("Bookmark", "Blue first fire"),
    "red_first_kill_flag": ("Bookmark", "Red first kill"),
    "blue_first_kill_flag": ("Bookmark", "Blue first kill"),
    "red_objective_complete_flag": ("Bookmark", "Red objective complete"),
    "blue_objective_complete_flag": ("Bookmark", "Blue objective complete"),
}


def _normalize_heading_deg(value: float) -> float:
    wrapped = float(value) % 360.0
    return wrapped / 180.0 - 1.0


def _denormalize_heading_deg(value: float) -> float:
    return math.fmod((float(value) + 1.0) * 180.0, 360.0)


def _encode_tactic_condition(condition: Dict) -> torch.Tensor:
    source_id = SOURCE_TO_ID.get(str(condition.get("source", "")), len(SOURCE_TO_ID))
    family_id = FAMILY_TO_ID.get(str(condition.get("family", "")), len(FAMILY_TO_ID))
    params = condition.get("params", {}) if isinstance(condition.get("params", {}), dict) else {}
    tactic_raw = params.get("tactic_id", condition.get("id", 0.0))
    try:
        tactic_id = float(tactic_raw)
    except (TypeError, ValueError):
        tactic_id = 0.0
    return torch.tensor(
        [
            tactic_id / 32.0,
            float(source_id) / 8.0,
            float(family_id) / 8.0,
        ],
        dtype=torch.float32,
    )


def _encode_side_units(units: List[Dict]) -> Tuple[List[str], torch.Tensor, torch.Tensor, torch.Tensor]:
    unit_ids: List[str] = []
    features: List[List[float]] = []
    type_ids: List[int] = []

    for unit in units:
        unit_id = str(unit.get("unit_id", ""))
        if not unit_id:
            continue
        unit_ids.append(unit_id)
        type_ids.append(int(round(float(unit.get("type_id", 0.0)))))
        features.append(
            [
                float(unit.get("alive", 0.0)),
                float(unit.get("missile_count", 0.0)) / MISSILE_SCALE,
                float(unit.get("lon", 0.0)) / LON_SCALE_DEG,
                float(unit.get("lat", 0.0)) / LAT_SCALE_DEG,
                float(unit.get("alt_m", 0.0)) / ALT_SCALE_M,
                float(unit.get("speed_mps", 0.0)) / SPEED_SCALE_MPS,
                _normalize_heading_deg(float(unit.get("heading_deg", 0.0))),
            ]
        )

    if not unit_ids:
        return (
            [],
            torch.zeros((1, 1, 7), dtype=torch.float32),
            torch.zeros((1, 1), dtype=torch.long),
            torch.zeros((1, 1), dtype=torch.bool),
        )

    return (
        unit_ids,
        torch.tensor([features], dtype=torch.float32),
        torch.tensor([type_ids], dtype=torch.long),
        torch.ones((1, len(unit_ids)), dtype=torch.bool),
    )


def _prepare_model_batch(
    state_t: Dict,
    red_tactic_condition: Dict,
    blue_tactic_condition: Dict,
    horizon: int,
    device: torch.device,
) -> Tuple[Dict[str, torch.Tensor], Dict[str, List[str]]]:
    red_ids, red_units, red_types, red_mask = _encode_side_units(state_t.get("red_units", []))
    blue_ids, blue_units, blue_types, blue_mask = _encode_side_units(state_t.get("blue_units", []))

    if horizon not in HORIZON_TO_ID:
        raise ValueError(f"Unsupported horizon: {horizon}. Supported values: {sorted(k for k in HORIZON_TO_ID if isinstance(k, int))}")

    batch = {
        "red_units": red_units.to(device),
        "red_unit_types": red_types.to(device),
        "red_mask": red_mask.to(device),
        "blue_units": blue_units.to(device),
        "blue_unit_types": blue_types.to(device),
        "blue_mask": blue_mask.to(device),
        "red_tactic": _encode_tactic_condition(red_tactic_condition).unsqueeze(0).to(device),
        "blue_tactic": _encode_tactic_condition(blue_tactic_condition).unsqueeze(0).to(device),
        "horizon_id": torch.tensor([HORIZON_TO_ID[horizon]], dtype=torch.long, device=device),
    }
    unit_id_map = {"red": red_ids, "blue": blue_ids}
    return batch, unit_id_map


def _decode_side_prediction(
    side: str,
    prev_units: List[Dict],
    unit_ids: List[str],
    alive_logits: torch.Tensor,
    reg_pred: torch.Tensor,
    alive_threshold: float,
) -> List[Dict]:
    prev_lookup = {str(unit.get("unit_id", "")): unit for unit in prev_units}
    decoded_units: List[Dict] = []

    for index, unit_id in enumerate(unit_ids):
        prev = prev_lookup.get(unit_id, {})
        prev_alive = bool(prev.get("alive", 0))
        prev_missile = int(max(0, round(float(prev.get("missile_count", 0)))))

        alive_prob = float(torch.sigmoid(alive_logits[index]).item())
        alive = prev_alive and (alive_prob >= alive_threshold)

        missile_raw = float(reg_pred[index, 0].item()) * MISSILE_SCALE
        missile_count = max(0, int(round(missile_raw)))
        missile_count = min(missile_count, prev_missile)
        if not alive:
            missile_count = 0

        lon = max(-180.0, min(180.0, float(reg_pred[index, 1].item()) * LON_SCALE_DEG))
        lat = max(-90.0, min(90.0, float(reg_pred[index, 2].item()) * LAT_SCALE_DEG))
        alt_m = max(0.0, float(reg_pred[index, 3].item()) * ALT_SCALE_M)
        speed_mps = max(0.0, float(reg_pred[index, 4].item()) * SPEED_SCALE_MPS)
        heading_deg = _denormalize_heading_deg(float(reg_pred[index, 5].item()))

        decoded_units.append(
            {
                "unit_id": unit_id,
                "type_id": float(prev.get("type_id", 0.0)),
                "alive": 1 if alive else 0,
                "missile_count": missile_count,
                "lon": lon,
                "lat": lat,
                "alt_m": alt_m,
                "speed_mps": speed_mps,
                "heading_deg": heading_deg,
                "side": side,
            }
        )

    return decoded_units


def _decode_next_state(
    current_state: Dict,
    unit_ids: Dict[str, List[str]],
    outputs: Dict[str, torch.Tensor],
    alive_threshold: float,
) -> Dict:
    red_units = _decode_side_prediction(
        side="red",
        prev_units=current_state.get("red_units", []),
        unit_ids=unit_ids["red"],
        alive_logits=outputs["red_alive_logit"][0],
        reg_pred=outputs["red_traj_reg"][0],
        alive_threshold=alive_threshold,
    )
    blue_units = _decode_side_prediction(
        side="blue",
        prev_units=current_state.get("blue_units", []),
        unit_ids=unit_ids["blue"],
        alive_logits=outputs["blue_alive_logit"][0],
        reg_pred=outputs["blue_traj_reg"][0],
        alive_threshold=alive_threshold,
    )
    return {"red_units": red_units, "blue_units": blue_units}


def _extract_step_signals(outputs: Dict[str, torch.Tensor]) -> Dict:
    event_probs: Dict[str, float] = {}
    for index, key in enumerate(EVENT_FLAG_KEYS):
        event_probs[key] = float(torch.sigmoid(outputs["event_flag_logits"][0, index]).item())

    event_counts: Dict[str, float] = {}
    for index, key in enumerate(EVENT_COUNT_KEYS):
        event_counts[key] = float(outputs["event_count_pred"][0, index].item())

    reward_step = {
        "red": float(outputs["reward_pred"][0, 0].item()),
        "blue": float(outputs["reward_pred"][0, 1].item()),
    }
    return {
        "event_probs": event_probs,
        "event_counts": event_counts,
        "reward_step": reward_step,
        "red_win_prob": float(torch.sigmoid(outputs["red_win_logit"][0]).item()),
    }


def _build_step_events(
    step_index: int,
    signals: Dict,
    emitted_once_flags: Set[str],
    last_retarget_step: Dict[str, int],
    event_prob_threshold: float,
    retarget_cooldown_steps: int,
) -> Tuple[List[TrajectoryEvent], bool]:
    events: List[TrajectoryEvent] = []
    event_probs = signals["event_probs"]
    event_counts = signals["event_counts"]

    def add(event_type: str, text: str) -> None:
        events.append(TrajectoryEvent(event_type=event_type, text=text))

    for key, (event_type, text) in FIRST_EVENT_TEXT.items():
        if event_probs.get(key, 0.0) >= event_prob_threshold and key not in emitted_once_flags:
            add(event_type, text)
            emitted_once_flags.add(key)

    for side in ("red", "blue"):
        key = f"{side}_retarget_flag"
        prob = event_probs.get(key, 0.0)
        if prob < event_prob_threshold:
            continue
        if step_index - last_retarget_step.get(side, -10**9) < retarget_cooldown_steps:
            continue
        add("Message", f"{side.capitalize()} retarget")
        last_retarget_step[side] = step_index

    red_fire = max(0, int(round(event_counts.get("red_fire_count_delta", 0.0))))
    blue_fire = max(0, int(round(event_counts.get("blue_fire_count_delta", 0.0))))
    red_kill = max(0, int(round(event_counts.get("red_kill_delta", 0.0))))
    blue_kill = max(0, int(round(event_counts.get("blue_kill_delta", 0.0))))

    if red_fire > 0 and "red_first_fire_flag" not in emitted_once_flags:
        add("Message", f"Red fire delta +{red_fire}")
    if blue_fire > 0 and "blue_first_fire_flag" not in emitted_once_flags:
        add("Message", f"Blue fire delta +{blue_fire}")
    if red_kill > 0 and "red_first_kill_flag" not in emitted_once_flags:
        add("Message", f"Red kill delta +{red_kill}")
    if blue_kill > 0 and "blue_first_kill_flag" not in emitted_once_flags:
        add("Message", f"Blue kill delta +{blue_kill}")

    terminated = bool(event_probs.get("termination_flag", 0.0) >= event_prob_threshold)
    if terminated and "termination_flag" not in emitted_once_flags:
        add("Bookmark", "Termination predicted by world model")
        emitted_once_flags.add("termination_flag")

    return events, terminated


def _build_reward_events(
    step_index: int,
    reward_step: Dict[str, float],
    reward_cumulative: Dict[str, float],
    reward_spike_threshold: float,
    reward_report_interval: int,
) -> List[TrajectoryEvent]:
    events: List[TrajectoryEvent] = []
    red_step = float(reward_step.get("red", 0.0))
    blue_step = float(reward_step.get("blue", 0.0))
    energy = abs(red_step) + abs(blue_step)
    if energy >= reward_spike_threshold:
        events.append(
            TrajectoryEvent(
                event_type="Message",
                text=(
                    "Reward spike "
                    f"red={red_step:.2f} blue={blue_step:.2f}; "
                    f"cum red={reward_cumulative.get('red', 0.0):.2f} blue={reward_cumulative.get('blue', 0.0):.2f}"
                ),
            )
        )

    if reward_report_interval > 0 and (step_index % reward_report_interval == 0):
        advantage = reward_cumulative.get("red", 0.0) - reward_cumulative.get("blue", 0.0)
        events.append(
            TrajectoryEvent(
                event_type="Message",
                text=(
                    "Reward trend "
                    f"cum red={reward_cumulative.get('red', 0.0):.2f} "
                    f"blue={reward_cumulative.get('blue', 0.0):.2f} adv={advantage:.2f}"
                ),
            )
        )
    return events


def _build_adaptive_event_fallbacks(
    step_signals: List[Dict],
    adaptive_min_prob: float,
) -> Dict[int, List[TrajectoryEvent]]:
    fallbacks: Dict[int, List[TrajectoryEvent]] = {}

    for key, (event_type, base_text) in FIRST_EVENT_TEXT.items():
        best_prob = -1.0
        best_step = -1
        for step_idx, step in enumerate(step_signals, start=1):
            prob = float(step["event_probs"].get(key, 0.0))
            if prob > best_prob:
                best_prob = prob
                best_step = step_idx
        if best_step >= 1 and best_prob >= adaptive_min_prob:
            text = f"Likely {base_text.lower()} (p={best_prob:.3f})"
            fallbacks.setdefault(best_step, []).append(TrajectoryEvent(event_type=event_type, text=text))

    for side in ("red", "blue"):
        key = f"{side}_retarget_flag"
        best_prob = -1.0
        best_step = -1
        for step_idx, step in enumerate(step_signals, start=1):
            prob = float(step["event_probs"].get(key, 0.0))
            if prob > best_prob:
                best_prob = prob
                best_step = step_idx
        if best_step >= 1 and best_prob >= adaptive_min_prob:
            text = f"Likely {side.capitalize()} retarget (p={best_prob:.3f})"
            fallbacks.setdefault(best_step, []).append(TrajectoryEvent(event_type="Message", text=text))

    best_prob = -1.0
    best_step = -1
    for step_idx, step in enumerate(step_signals, start=1):
        prob = float(step["event_probs"].get("termination_flag", 0.0))
        if prob > best_prob:
            best_prob = prob
            best_step = step_idx
    if best_step >= 1 and best_prob >= adaptive_min_prob:
        text = f"Likely termination signal (p={best_prob:.3f})"
        fallbacks.setdefault(best_step, []).append(TrajectoryEvent(event_type="Bookmark", text=text))

    return fallbacks


def _state_to_objects(state: Dict) -> Dict[str, TrajectoryObjectState]:
    objects: Dict[str, TrajectoryObjectState] = {}
    for side in ("red", "blue"):
        for unit in state.get(f"{side}_units", []):
            unit_id = str(unit["unit_id"])
            objects[unit_id] = TrajectoryObjectState(
                object_key=unit_id,
                side=side,
                lon=float(unit.get("lon", 0.0)),
                lat=float(unit.get("lat", 0.0)),
                alt_m=float(unit.get("alt_m", 0.0)),
                speed_mps=float(unit.get("speed_mps", 0.0)),
                heading_deg=float(unit.get("heading_deg", 0.0)) % 360.0,
                alive=bool(unit.get("alive", 0)),
                missile_count=max(0, int(unit.get("missile_count", 0))),
                type_id=str(unit.get("type_id", "unknown")),
            )
    return objects


def load_seed_record(
    processed_path: Path,
    episode_id: Optional[str],
    state_step: int,
    horizon: int,
) -> Dict:
    records = read_jsonl(processed_path)
    if not records:
        raise ValueError(f"No processed records found in {processed_path}")

    candidates: List[Dict] = []
    for record in records:
        record_horizon = record.get("horizon", -1)
        if not isinstance(record_horizon, int):
            continue
        if int(record_horizon) != int(horizon):
            continue
        meta = record.get("meta", {}) or {}
        if int(meta.get("state_step", -1)) != int(state_step):
            continue
        if episode_id is not None and str(meta.get("episode_id")) != str(episode_id):
            continue
        candidates.append(record)

    if not candidates:
        raise ValueError(
            f"No matching processed sample found in {processed_path} with episode_id={episode_id}, horizon={horizon}, state_step={state_step}"
        )
    return candidates[0]


def rollout_predicted_trajectory(
    model: WorldModelNet,
    initial_state: Dict,
    red_tactic_condition: Dict,
    blue_tactic_condition: Dict,
    horizon: int,
    rollout_steps: int,
    dt_s: float,
    alive_threshold: float,
    event_prob_threshold: float,
    device: torch.device,
) -> Tuple[List[Dict], List[Dict]]:
    if rollout_steps <= 0:
        raise ValueError("rollout_steps must be positive.")

    states: List[Dict] = [initial_state]
    step_signals: List[Dict] = []
    current_state = initial_state

    for _ in range(rollout_steps):
        batch, unit_ids = _prepare_model_batch(current_state, red_tactic_condition, blue_tactic_condition, horizon, device)
        with torch.no_grad():
            outputs = model(batch)
        signals = _extract_step_signals(outputs)
        signals["termination_likely"] = bool(signals["event_probs"].get("termination_flag", 0.0) >= event_prob_threshold)
        step_signals.append(signals)

        next_state = _decode_next_state(current_state, unit_ids, outputs, alive_threshold=alive_threshold)
        states.append(next_state)
        current_state = next_state

    _ = dt_s
    return states, step_signals


def build_predicted_episode(
    seed_record: Dict,
    states: List[Dict],
    step_signals: List[Dict],
    dt_s: float,
    episode_suffix: str,
    event_prob_threshold: float,
    retarget_cooldown_steps: int,
    reward_spike_threshold: float,
    reward_report_interval: int,
    adaptive_event_fallback: bool,
    adaptive_min_prob: float,
) -> TrajectoryEpisode:
    meta = seed_record.get("meta", {}) or {}
    task_id = str(meta.get("task_id", "pred_task"))
    source_episode_id = str(meta.get("episode_id", "pred_episode"))
    episode_id = f"{source_episode_id}{episode_suffix}"

    final_red_win_prob = step_signals[-1]["red_win_prob"] if step_signals else 0.5
    predicted_termination_seen = any(bool(step.get("termination_likely", False)) for step in step_signals)
    final_reason = "predicted_termination_flag" if predicted_termination_seen else "predicted_rollout_horizon"
    outcome = {
        "termination_reason": final_reason,
        "red_win_prob": final_red_win_prob,
        "red_win": 1 if final_red_win_prob >= 0.5 else 0,
    }

    frames: List[TrajectoryFrame] = []
    emitted_once_flags: Set[str] = set()
    last_retarget_step: Dict[str, int] = {"red": -10**9, "blue": -10**9}
    reward_cumulative = {"red": 0.0, "blue": 0.0}
    fallback_events: Dict[int, List[TrajectoryEvent]] = {}
    if adaptive_event_fallback:
        fallback_events = _build_adaptive_event_fallbacks(step_signals, adaptive_min_prob=adaptive_min_prob)

    for index, state in enumerate(states):
        sim_time_s = float(index) * dt_s
        events: List[TrajectoryEvent] = []
        done = False
        termination_reason = "none"

        reward_step = {"red": 0.0, "blue": 0.0}
        if index > 0 and (index - 1) < len(step_signals):
            signals = step_signals[index - 1]
            reward_step = dict(signals["reward_step"])
            reward_cumulative["red"] += float(reward_step.get("red", 0.0))
            reward_cumulative["blue"] += float(reward_step.get("blue", 0.0))

            step_events, _ = _build_step_events(
                step_index=index,
                signals=signals,
                emitted_once_flags=emitted_once_flags,
                last_retarget_step=last_retarget_step,
                event_prob_threshold=event_prob_threshold,
                retarget_cooldown_steps=retarget_cooldown_steps,
            )
            events.extend(step_events)
            events.extend(
                _build_reward_events(
                    step_index=index,
                    reward_step=reward_step,
                    reward_cumulative=reward_cumulative,
                    reward_spike_threshold=reward_spike_threshold,
                    reward_report_interval=reward_report_interval,
                )
            )

            for event in fallback_events.get(index, []):
                events.append(event)

        if index == len(states) - 1:
            done = True
            termination_reason = final_reason
            events.append(
                TrajectoryEvent(
                    event_type="Bookmark",
                    text=(
                        f"Predicted rollout end; reason={final_reason}; "
                        f"red_win_prob={final_red_win_prob:.3f}; "
                        f"cum_reward red={reward_cumulative.get('red', 0.0):.2f} blue={reward_cumulative.get('blue', 0.0):.2f}"
                    ),
                )
            )

        frames.append(
            TrajectoryFrame(
                sim_time_s=sim_time_s,
                objects=_state_to_objects(state),
                events=events,
                termination_reason=termination_reason,
                done=done,
                reward_step=reward_step,
                reward_cumulative=dict(reward_cumulative),
            )
        )

    sampling_meta = dict(meta)
    sampling_meta["prediction_mode"] = "world_model_rollout"
    sampling_meta["num_predicted_steps"] = max(len(states) - 1, 0)

    return TrajectoryEpisode(
        task_id=task_id,
        episode_id=episode_id,
        red_tactic_condition=dict(seed_record.get("red_tactic_condition", {})),
        blue_tactic_condition=dict(seed_record.get("blue_tactic_condition", {})),
        episode_outcome=outcome,
        sampling_meta=sampling_meta,
        frames=frames,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate predicted trajectory ACMI using world model rollout.")
    parser.add_argument("--processed-input", type=Path, required=True, help="Processed world model JSONL path.")
    parser.add_argument("--model-path", type=Path, required=True, help="Trained world model checkpoint path.")
    parser.add_argument("--output", type=Path, default=None, help="Output .txt.acmi path.")
    parser.add_argument("--episode-id", type=str, default=None, help="Episode id to use as initial state.")
    parser.add_argument("--state-step", type=int, default=0, help="state_step in processed sample.")
    parser.add_argument("--horizon", type=int, default=1, choices=[1, 5, 10, 20], help="Model horizon conditioning id.")
    parser.add_argument("--rollout-steps", type=int, default=80, help="Number of predicted rollout steps.")
    parser.add_argument("--dt-s", type=float, default=1.0, help="Frame delta time in seconds.")
    parser.add_argument("--alive-threshold", type=float, default=0.5)
    parser.add_argument("--event-prob-threshold", type=float, default=0.60)
    parser.add_argument("--retarget-cooldown-steps", type=int, default=8)
    parser.add_argument("--reward-spike-threshold", type=float, default=1.0)
    parser.add_argument("--reward-report-interval", type=int, default=10)
    parser.add_argument("--disable-adaptive-event-fallback", action="store_true")
    parser.add_argument("--adaptive-min-prob", type=float, default=0.005)
    parser.add_argument("--device", type=str, default="cpu", choices=["cpu", "cuda"])
    parser.add_argument("--interpolate", action="store_true", help="Enable ACMI interpolation.")
    parser.add_argument("--interpolate-step-s", type=float, default=0.25)
    args = parser.parse_args()

    if args.device == "cuda" and not torch.cuda.is_available():
        raise RuntimeError("CUDA requested but not available in current PyTorch runtime.")
    device = torch.device(args.device)

    seed_record = load_seed_record(
        processed_path=args.processed_input,
        episode_id=args.episode_id,
        state_step=args.state_step,
        horizon=args.horizon,
    )

    model = WorldModelNet().to(device)
    model.load_state_dict(torch.load(args.model_path, map_location=device))
    model.eval()

    initial_state = dict(seed_record.get("state_t", {}))
    red_tactic_condition = dict(seed_record.get("red_tactic_condition", {}))
    blue_tactic_condition = dict(seed_record.get("blue_tactic_condition", {}))

    states, step_signals = rollout_predicted_trajectory(
        model=model,
        initial_state=initial_state,
        red_tactic_condition=red_tactic_condition,
        blue_tactic_condition=blue_tactic_condition,
        horizon=args.horizon,
        rollout_steps=args.rollout_steps,
        dt_s=args.dt_s,
        alive_threshold=args.alive_threshold,
        event_prob_threshold=args.event_prob_threshold,
        device=device,
    )

    episode_suffix = f"_pred_h{args.horizon}_n{args.rollout_steps}"
    episode = build_predicted_episode(
        seed_record=seed_record,
        states=states,
        step_signals=step_signals,
        dt_s=args.dt_s,
        episode_suffix=episode_suffix,
        event_prob_threshold=args.event_prob_threshold,
        retarget_cooldown_steps=args.retarget_cooldown_steps,
        reward_spike_threshold=args.reward_spike_threshold,
        reward_report_interval=args.reward_report_interval,
        adaptive_event_fallback=(not args.disable_adaptive_event_fallback),
        adaptive_min_prob=args.adaptive_min_prob,
    )

    if args.output is None:
        episode_key = str((seed_record.get("meta", {}) or {}).get("episode_id", "episode"))
        output_path = args.processed_input.parent.parent / "acmi" / f"predicted_{episode_key}{episode_suffix}.txt.acmi"
    else:
        output_path = args.output

    write_acmi(
        episode,
        output_path,
        interpolate=args.interpolate,
        interpolate_step_s=args.interpolate_step_s,
    )

    print(f"Predicted ACMI written to: {output_path}")
    print(f"Seed episode: {(seed_record.get('meta', {}) or {}).get('episode_id', 'unknown')}")
    print(f"Frames: {len(episode.frames)}")
    print(f"Final predicted red_win_prob: {(step_signals[-1]['red_win_prob'] if step_signals else 0.5):.4f}")


if __name__ == "__main__":
    main()
