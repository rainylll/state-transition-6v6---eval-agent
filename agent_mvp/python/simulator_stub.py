import math
import random
from typing import Dict, List, Tuple

UNIT_TYPES = ("fighter", "ship")
TACTICS = (
    "launch_timing",
    "formation",
    "target_assignment",
    "attack_axis",
    "support_style",
)


def _sample_unit(rng: random.Random, unit_type: str) -> Dict[str, float]:
    """Sample one unit in a unified capability space."""
    if unit_type == "fighter":
        speed = rng.uniform(180.0, 360.0)
        sensor = rng.uniform(0.4, 0.9)
        weapon = rng.uniform(0.4, 1.0)
        defense = rng.uniform(0.2, 0.7)
        hp = rng.uniform(0.5, 0.9)
    else:
        speed = rng.uniform(20.0, 45.0)
        sensor = rng.uniform(0.5, 1.0)
        weapon = rng.uniform(0.5, 1.0)
        defense = rng.uniform(0.6, 1.0)
        hp = rng.uniform(0.8, 1.2)

    return {
        "type": unit_type,
        "speed": speed,
        "sensor": sensor,
        "weapon": weapon,
        "defense": defense,
        "hp": hp,
        "ecm": rng.uniform(0.1, 0.9),
    }


def _sample_side(rng: random.Random, fighter_range: Tuple[int, int], ship_range: Tuple[int, int]) -> List[Dict[str, float]]:
    units: List[Dict[str, float]] = []
    fighters = rng.randint(fighter_range[0], fighter_range[1])
    ships = rng.randint(ship_range[0], ship_range[1])
    for _ in range(fighters):
        units.append(_sample_unit(rng, "fighter"))
    for _ in range(ships):
        units.append(_sample_unit(rng, "ship"))
    return units


def _power(units: List[Dict[str, float]]) -> float:
    total = 0.0
    for u in units:
        speed_term = math.log(max(u["speed"], 1.0))
        total += (
            0.7 * u["weapon"]
            + 0.5 * u["sensor"]
            + 0.4 * u["defense"]
            + 0.3 * u["ecm"]
            + 0.2 * speed_term
            + 0.5 * u["hp"]
        )
    return total


def _tactic_vector(rng: random.Random) -> List[float]:
    return [rng.random() for _ in TACTICS]


def _label_episode(
    rng: random.Random,
    red_units: List[Dict[str, float]],
    blue_units: List[Dict[str, float]],
    tactic: List[float],
) -> Dict[str, float]:
    red_power = _power(red_units)
    blue_power = _power(blue_units)

    # Coarse tactical gain terms to mimic command effect in the first MVP.
    red_gain = 1.0 + 0.20 * tactic[1] + 0.15 * tactic[2] + 0.10 * tactic[4]
    blue_gain = 1.0 + rng.uniform(0.0, 0.15)

    diff = red_power * red_gain - blue_power * blue_gain + rng.gauss(0.0, 0.8)
    p_red_win = 1.0 / (1.0 + math.exp(-diff / 3.5))
    red_win = 1.0 if rng.random() < p_red_win else 0.0

    # Attrition ratio in [0,1], lower is better.
    red_loss = max(0.0, min(1.0, 0.70 - 0.55 * p_red_win + rng.gauss(0.0, 0.04)))
    blue_loss = max(0.0, min(1.0, 0.25 + 0.55 * p_red_win + rng.gauss(0.0, 0.04)))

    return {
        "p_red_win": p_red_win,
        "red_win": red_win,
        "red_loss": red_loss,
        "blue_loss": blue_loss,
    }


def sample_episode(rng: random.Random, config: Dict) -> Dict:
    red_units = _sample_side(
        rng,
        tuple(config["red"]["fighters"]),
        tuple(config["red"]["ships"]),
    )
    blue_units = _sample_side(
        rng,
        tuple(config["blue"]["fighters"]),
        tuple(config["blue"]["ships"]),
    )
    tactic = _tactic_vector(rng)
    labels = _label_episode(rng, red_units, blue_units, tactic)

    return {
        "red_units": red_units,
        "blue_units": blue_units,
        "tactic": tactic,
        "labels": labels,
    }

