import argparse
import math
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

from data_io import read_jsonl

FILE_HEADER = (
    "FileType=text/acmi/tacview",
    "FileVersion=2.2",
)
DEFAULT_REFERENCE_TIME = "2026-01-01T00:00:00Z"
SUPPORTED_SIDES = ("red", "blue")
EVENT_PRIORITY = (
    "red_first_contact_flag",
    "blue_first_contact_flag",
    "red_first_fire_flag",
    "blue_first_fire_flag",
    "red_warning_flag",
    "blue_warning_flag",
    "red_retarget_flag",
    "blue_retarget_flag",
    "red_first_kill_flag",
    "blue_first_kill_flag",
    "red_objective_complete_flag",
    "blue_objective_complete_flag",
    "termination_flag",
)


@dataclass
class TrajectoryObjectState:
    object_key: str
    side: str
    lon: float
    lat: float
    alt_m: float
    speed_mps: float
    heading_deg: float
    alive: bool
    missile_count: int
    type_id: str


@dataclass
class TrajectoryEvent:
    event_type: str
    text: str
    object_keys: List[str] = field(default_factory=list)


@dataclass
class TrajectoryFrame:
    sim_time_s: float
    objects: Dict[str, TrajectoryObjectState]
    events: List[TrajectoryEvent] = field(default_factory=list)
    termination_reason: str = "none"
    done: bool = False
    reward_step: Dict[str, float] = field(default_factory=dict)
    reward_cumulative: Dict[str, float] = field(default_factory=dict)


@dataclass
class TrajectoryEpisode:
    task_id: str
    episode_id: str
    red_tactic_condition: Dict
    blue_tactic_condition: Dict
    episode_outcome: Dict
    sampling_meta: Dict
    frames: List[TrajectoryFrame]


def _escape_acmi_text(text: str) -> str:
    sanitized = text.replace("\\", "\\\\").replace(",", "\\,").replace("\n", " ").replace("\r", " ")
    return sanitized.replace("|", "/")


def _coerce_float(value: object, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _coerce_int(value: object, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _normalize_heading_deg(value: object) -> float:
    heading = _coerce_float(value)
    heading = math.fmod(heading, 360.0)
    if heading < 0.0:
        heading += 360.0
    return heading


def _normalize_side(side: str) -> str:
    side_lower = str(side).lower()
    if side_lower not in SUPPORTED_SIDES:
        raise ValueError(f"Unsupported side: {side}")
    return side_lower


def _parse_unit(unit: Dict, side: str) -> TrajectoryObjectState:
    return TrajectoryObjectState(
        object_key=str(unit["unit_id"]),
        side=_normalize_side(side),
        lon=_coerce_float(unit["lon"]),
        lat=_coerce_float(unit["lat"]),
        alt_m=_coerce_float(unit["alt_m"]),
        speed_mps=_coerce_float(unit["speed_mps"]),
        heading_deg=_normalize_heading_deg(unit["heading_deg"]),
        alive=bool(unit["alive"]),
        missile_count=max(0, _coerce_int(unit["missile_count"])),
        type_id=str(unit.get("type_id", "unknown")),
    )


def _state_to_objects(state: Dict) -> Dict[str, TrajectoryObjectState]:
    objects: Dict[str, TrajectoryObjectState] = {}
    for side in SUPPORTED_SIDES:
        side_units = state.get(f"{side}_units", [])
        for unit in side_units:
            parsed = _parse_unit(unit, side)
            objects[parsed.object_key] = parsed
    return objects


def _make_event_texts(row: Dict) -> List[TrajectoryEvent]:
    event_payload = row.get("event", {}) or {}
    reward_step = row.get("reward_step", {}) or {}
    reward_cumulative = row.get("reward_cumulative", {}) or {}
    events: List[TrajectoryEvent] = []

    def add(event_type: str, text: str) -> None:
        events.append(TrajectoryEvent(event_type=event_type, text=text))

    if event_payload.get("red_first_contact_flag"):
        add("Bookmark", "Red first contact")
    if event_payload.get("blue_first_contact_flag"):
        add("Bookmark", "Blue first contact")
    if event_payload.get("red_first_fire_flag"):
        add("Bookmark", "Red first fire")
    if event_payload.get("blue_first_fire_flag"):
        add("Bookmark", "Blue first fire")
    if event_payload.get("red_retarget_flag"):
        add("Message", "Red retarget")
    if event_payload.get("blue_retarget_flag"):
        add("Message", "Blue retarget")
    if event_payload.get("red_first_kill_flag"):
        add("Bookmark", "Red first kill")
    if event_payload.get("blue_first_kill_flag"):
        add("Bookmark", "Blue first kill")
    if event_payload.get("red_kill_delta", 0):
        add("Message", f"Red kill delta +{_coerce_int(event_payload.get('red_kill_delta'))}")
    if event_payload.get("blue_kill_delta", 0):
        add("Message", f"Blue kill delta +{_coerce_int(event_payload.get('blue_kill_delta'))}")
    if event_payload.get("red_fire_count_delta", 0) and not event_payload.get("red_first_fire_flag"):
        add("Message", f"Red fire delta +{_coerce_int(event_payload.get('red_fire_count_delta'))}")
    if event_payload.get("blue_fire_count_delta", 0) and not event_payload.get("blue_first_fire_flag"):
        add("Message", f"Blue fire delta +{_coerce_int(event_payload.get('blue_fire_count_delta'))}")
    if event_payload.get("red_objective_complete_flag"):
        add("Bookmark", "Red objective complete")
    if event_payload.get("blue_objective_complete_flag"):
        add("Bookmark", "Blue objective complete")

    red_reward = _coerce_float(reward_step.get("red", 0.0))
    blue_reward = _coerce_float(reward_step.get("blue", 0.0))
    reward_energy = abs(red_reward) + abs(blue_reward)
    if reward_energy >= 1.0:
        add(
            "Message",
            (
                "Reward spike "
                f"red={red_reward:.2f} blue={blue_reward:.2f}; "
                f"cum red={_coerce_float(reward_cumulative.get('red', 0.0)):.2f} "
                f"blue={_coerce_float(reward_cumulative.get('blue', 0.0)):.2f}"
            ),
        )

    if event_payload.get("termination_flag") or row.get("done", False):
        outcome = row.get("episode_outcome", {}) or {}
        red_win = outcome.get("red_win", "unknown")
        termination_reason = row.get("termination_reason", "none")
        add(
            "Bookmark",
            f"Termination {termination_reason}; red_win={red_win}; reward_step red={red_reward:.2f} blue={blue_reward:.2f}",
        )
    return events


def load_rollout_rows(path: Path, episode_id: Optional[str] = None) -> List[Dict]:
    rows = read_jsonl(path)
    if not rows:
        raise ValueError(f"No rows found in {path}")

    if episode_id is None:
        episode_ids = sorted({str(row["episode_id"]) for row in rows})
        if len(episode_ids) != 1:
            preview = ", ".join(episode_ids[:10])
            raise ValueError(
                f"Input contains multiple episodes; pass --episode-id. Available examples: {preview}"
            )
        episode_id = episode_ids[0]

    filtered = [row for row in rows if str(row["episode_id"]) == episode_id]
    if not filtered:
        raise ValueError(f"Episode {episode_id} not found in {path}")
    filtered.sort(key=lambda row: (_coerce_float(row.get("sim_time_s", 0.0)), _coerce_int(row.get("step", 0))))
    return filtered


def rollout_rows_to_trajectory(rows: Sequence[Dict]) -> TrajectoryEpisode:
    if not rows:
        raise ValueError("Cannot convert empty rollout rows.")

    first_row = rows[0]
    frames: List[TrajectoryFrame] = [
        TrajectoryFrame(
            sim_time_s=0.0,
            objects=_state_to_objects(first_row["state"]),
            events=[],
            done=False,
            termination_reason="none",
            reward_step={"red": 0.0, "blue": 0.0},
            reward_cumulative={"red": 0.0, "blue": 0.0},
        )
    ]

    for row in rows:
        frames.append(
            TrajectoryFrame(
                sim_time_s=_coerce_float(row["sim_time_s"]),
                objects=_state_to_objects(row["next_state"]),
                events=_make_event_texts(row),
                done=bool(row.get("done", False)),
                termination_reason=str(row.get("termination_reason", "none")),
                reward_step={
                    "red": _coerce_float((row.get("reward_step") or {}).get("red", 0.0)),
                    "blue": _coerce_float((row.get("reward_step") or {}).get("blue", 0.0)),
                },
                reward_cumulative={
                    "red": _coerce_float((row.get("reward_cumulative") or {}).get("red", 0.0)),
                    "blue": _coerce_float((row.get("reward_cumulative") or {}).get("blue", 0.0)),
                },
            )
        )

    return TrajectoryEpisode(
        task_id=str(first_row["task_id"]),
        episode_id=str(first_row["episode_id"]),
        red_tactic_condition=dict(first_row.get("red_tactic_condition", {})),
        blue_tactic_condition=dict(first_row.get("blue_tactic_condition", {})),
        episode_outcome=dict(first_row.get("episode_outcome", {})),
        sampling_meta=dict(first_row.get("sampling_meta", {})),
        frames=frames,
    )


def _interpolate_scalar(start: float, end: float, ratio: float) -> float:
    return start + (end - start) * ratio


def _interpolate_heading_deg(start: float, end: float, ratio: float) -> float:
    delta = (end - start + 540.0) % 360.0 - 180.0
    return _normalize_heading_deg(start + delta * ratio)


def interpolate_episode_frames(episode: TrajectoryEpisode, step_s: float) -> TrajectoryEpisode:
    if step_s <= 0.0:
        raise ValueError("Interpolation step must be positive.")
    if len(episode.frames) < 2:
        return episode

    interpolated_frames: List[TrajectoryFrame] = [episode.frames[0]]
    for left, right in zip(episode.frames, episode.frames[1:]):
        delta_t = right.sim_time_s - left.sim_time_s
        if delta_t <= step_s + 1e-9:
            interpolated_frames.append(right)
            continue

        active_keys = sorted(set(left.objects) | set(right.objects))
        current_t = left.sim_time_s + step_s
        while current_t < right.sim_time_s - 1e-9:
            ratio = (current_t - left.sim_time_s) / delta_t
            objects: Dict[str, TrajectoryObjectState] = {}
            for key in active_keys:
                left_obj = left.objects.get(key)
                right_obj = right.objects.get(key)
                if left_obj is None or right_obj is None:
                    continue
                objects[key] = TrajectoryObjectState(
                    object_key=key,
                    side=left_obj.side,
                    lon=_interpolate_scalar(left_obj.lon, right_obj.lon, ratio),
                    lat=_interpolate_scalar(left_obj.lat, right_obj.lat, ratio),
                    alt_m=_interpolate_scalar(left_obj.alt_m, right_obj.alt_m, ratio),
                    speed_mps=_interpolate_scalar(left_obj.speed_mps, right_obj.speed_mps, ratio),
                    heading_deg=_interpolate_heading_deg(left_obj.heading_deg, right_obj.heading_deg, ratio),
                    alive=left_obj.alive if ratio < 1.0 else right_obj.alive,
                    missile_count=left_obj.missile_count if ratio < 1.0 else right_obj.missile_count,
                    type_id=left_obj.type_id,
                )
            interpolated_frames.append(
                TrajectoryFrame(
                    sim_time_s=current_t,
                    objects=objects,
                    events=[],
                    termination_reason="none",
                    done=False,
                    reward_step={"red": 0.0, "blue": 0.0},
                    reward_cumulative=dict(left.reward_cumulative),
                )
            )
            current_t += step_s
        interpolated_frames.append(right)

    return TrajectoryEpisode(
        task_id=episode.task_id,
        episode_id=episode.episode_id,
        red_tactic_condition=dict(episode.red_tactic_condition),
        blue_tactic_condition=dict(episode.blue_tactic_condition),
        episode_outcome=dict(episode.episode_outcome),
        sampling_meta=dict(episode.sampling_meta),
        frames=interpolated_frames,
    )


def _build_object_id_map(frames: Sequence[TrajectoryFrame]) -> Dict[str, str]:
    side_groups: Dict[str, List[str]] = {side: [] for side in SUPPORTED_SIDES}
    seen: set[str] = set()
    for frame in frames:
        for key, state in frame.objects.items():
            if key in seen:
                continue
            seen.add(key)
            side_groups[state.side].append(key)

    mapping: Dict[str, str] = {}
    base_ids = {"red": 0x1000, "blue": 0x2000}
    for side in SUPPORTED_SIDES:
        for offset, object_key in enumerate(sorted(side_groups[side]), start=1):
            mapping[object_key] = format(base_ids[side] + offset, "X")
    return mapping


def _object_create_properties(state: TrajectoryObjectState) -> List[Tuple[str, object]]:
    coalition = "Red" if state.side == "red" else "Blue"
    color = "Red" if state.side == "red" else "Blue"
    return [
        ("Type", "Air+FixedWing"),
        ("Name", "Aircraft"),
        ("CallSign", state.object_key),
        ("Group", coalition),
        ("Coalition", coalition),
        ("Color", color),
    ]


def _object_dynamic_properties(state: TrajectoryObjectState) -> List[Tuple[str, object]]:
    label = f"{state.object_key} M{state.missile_count}"
    return [
        ("T", f"{state.lon:.8f}|{state.lat:.8f}|{state.alt_m:.2f}"),
        ("HDG", f"{state.heading_deg:.3f}"),
        ("TAS", f"{state.speed_mps:.3f}"),
        ("Health", "1" if state.alive else "0"),
        ("Label", label),
    ]


def _format_property_line(object_id: str, properties: Iterable[Tuple[str, object]]) -> str:
    payload: List[str] = [object_id]
    for key, value in properties:
        text = str(value)
        if key in {"Name", "CallSign", "Group", "Coalition", "Color", "Label"}:
            text = _escape_acmi_text(text)
        payload.append(f"{key}={text}")
    return ",".join(payload)


def _format_event_line(event: TrajectoryEvent, object_ids: Dict[str, str]) -> str:
    tokens = [event.event_type]
    for object_key in event.object_keys:
        object_id = object_ids.get(object_key)
        if object_id:
            tokens.append(object_id)
    tokens.append(_escape_acmi_text(event.text))
    return "0,Event=" + "|".join(tokens)


def serialize_acmi_lines(
    episode: TrajectoryEpisode,
    reference_time: str = DEFAULT_REFERENCE_TIME,
    recording_time: Optional[str] = None,
) -> List[str]:
    lines: List[str] = list(FILE_HEADER)
    recorded_at = recording_time or datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    comments = (
        f"episode={episode.episode_id}; red_tactic={episode.red_tactic_condition.get('id', 'unknown')}; "
        f"blue_tactic={episode.blue_tactic_condition.get('id', 'unknown')}; "
        f"termination={episode.episode_outcome.get('termination_reason', 'unknown')}; "
        f"red_win={episode.episode_outcome.get('red_win', 'unknown')}"
    )
    lines.extend(
        [
            f"0,ReferenceTime={reference_time}",
            f"0,RecordingTime={recorded_at}",
            f"0,Title={_escape_acmi_text(episode.episode_id)}",
            "0,Category=Air Combat",
            "0,DataSource=agent_mvp rollouts.jsonl",
            "0,DataRecorder=trajectory_to_acmi.py",
            f"0,Comments={_escape_acmi_text(comments)}",
        ]
    )

    object_ids = _build_object_id_map(episode.frames)
    active_objects: Dict[str, bool] = {}
    prior_objects: Dict[str, TrajectoryObjectState] = {}
    retired_objects: set[str] = set()

    for frame in episode.frames:
        lines.append(f"#{frame.sim_time_s:.2f}")
        current_keys = set(frame.objects)

        for object_key in sorted(current_keys):
            state = frame.objects[object_key]
            if object_key in retired_objects and not state.alive:
                continue
            object_id = object_ids[object_key]
            properties = []
            if object_key not in active_objects:
                properties.extend(_object_create_properties(state))
            properties.extend(_object_dynamic_properties(state))
            lines.append(_format_property_line(object_id, properties))

            previous_state = prior_objects.get(object_key)
            if previous_state is not None and previous_state.alive and not state.alive:
                lines.append(_format_event_line(TrajectoryEvent("Destroyed", f"{object_key} destroyed", [object_key]), object_ids))
                lines.append(f"-{object_id}")
                active_objects.pop(object_key, None)
                retired_objects.add(object_key)
            else:
                active_objects[object_key] = state.alive

        missing_keys = [key for key in list(active_objects) if key not in current_keys]
        for object_key in sorted(missing_keys):
            lines.append(f"-{object_ids[object_key]}")
            active_objects.pop(object_key, None)
            retired_objects.add(object_key)

        for event in frame.events:
            lines.append(_format_event_line(event, object_ids))

        if frame.done and frame.termination_reason != "none":
            text = (
                f"Episode done; termination={frame.termination_reason}; "
                f"red_reward={frame.reward_cumulative.get('red', 0.0):.2f}; "
                f"blue_reward={frame.reward_cumulative.get('blue', 0.0):.2f}"
            )
            lines.append(_format_event_line(TrajectoryEvent("Bookmark", text), object_ids))

        prior_objects = dict(frame.objects)
    return lines


def write_acmi(episode: TrajectoryEpisode, output_path: Path, interpolate: bool = False, interpolate_step_s: float = 0.25) -> Path:
    export_episode = episode
    if interpolate:
        export_episode = interpolate_episode_frames(episode, interpolate_step_s)

    lines = serialize_acmi_lines(export_episode)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8-sig", newline="\n") as handle:
        for line in lines:
            handle.write(line)
            handle.write("\n")
    return output_path


def build_output_path(input_path: Path, episode_id: str, output_path: Optional[Path]) -> Path:
    if output_path is not None:
        return output_path
    safe_episode_id = "".join(ch if ch.isalnum() or ch in {"-", "_"} else "_" for ch in episode_id)
    return input_path.parent.parent / "acmi" / f"{input_path.stem}__{safe_episode_id}.txt.acmi"


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert structured rollout JSONL into a Tacview ACMI file.")
    parser.add_argument("--input", type=Path, required=True, help="Path to rollouts.jsonl")
    parser.add_argument("--episode-id", type=str, default=None, help="Episode id to export when input has multiple episodes.")
    parser.add_argument("--output", type=Path, default=None, help="Path to output .txt.acmi file.")
    parser.add_argument("--reference-time", type=str, default=DEFAULT_REFERENCE_TIME, help="ACMI ReferenceTime, UTC ISO-8601.")
    parser.add_argument("--interpolate", action="store_true", help="Enable simple linear interpolation between 1s rollout frames.")
    parser.add_argument(
        "--interpolate-step-s",
        type=float,
        default=0.25,
        help="Interpolation step in seconds when --interpolate is enabled.",
    )
    args = parser.parse_args()

    rows = load_rollout_rows(args.input, args.episode_id)
    episode = rollout_rows_to_trajectory(rows)
    output_path = build_output_path(args.input, episode.episode_id, args.output)
    write_acmi(episode, output_path, interpolate=args.interpolate, interpolate_step_s=args.interpolate_step_s)

    print(f"ACMI written to: {output_path}")
    print(f"Episode: {episode.episode_id}")
    print(f"Frames: {len(episode.frames)}")
    print(f"Termination: {episode.episode_outcome.get('termination_reason', 'unknown')}")
    print(
        "Tactics:"
        f" red={episode.red_tactic_condition.get('family', 'unknown')}:{episode.red_tactic_condition.get('id', 'unknown')}"
        f" blue={episode.blue_tactic_condition.get('family', 'unknown')}:{episode.blue_tactic_condition.get('id', 'unknown')}"
    )


if __name__ == "__main__":
    main()
