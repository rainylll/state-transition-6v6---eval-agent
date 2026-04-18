from typing import Dict

import torch
import torch.nn as nn

from world_model_dataset import (
    EVENT_COUNT_DIM,
    EVENT_FLAG_KEYS,
    EVENT_FLAG_DIM,
    REWARD_DIM,
    TACTIC_FEATURE_DIM,
    TERMINAL_CRITICAL_ROLE_DIM,
    TERMINAL_SELF_ROLE_DIM,
    UNIT_FEATURE_DIM,
)

RED_FIRST_KILL_EVENT_INDEX = EVENT_FLAG_KEYS.index("red_first_kill_flag")
BLUE_FIRST_KILL_EVENT_INDEX = EVENT_FLAG_KEYS.index("blue_first_kill_flag")
SELF_FIRST_KILL_ROLE_INDEX = 0
DERIVED_LOGIT_EPS = 1e-4


def masked_mean(x: torch.Tensor, mask: torch.Tensor) -> torch.Tensor:
    weights = mask.unsqueeze(-1).float()
    denom = weights.sum(dim=1).clamp(min=1.0)
    return (x * weights).sum(dim=1) / denom


class UnitEncoder(nn.Module):
    def __init__(self, hidden_dim: int, num_type_buckets: int = 32, type_embed_dim: int = 8):
        super().__init__()
        self.type_emb = nn.Embedding(num_type_buckets, type_embed_dim)
        self.cont_mlp = nn.Sequential(
            nn.Linear(UNIT_FEATURE_DIM, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
        )
        self.proj = nn.Linear(hidden_dim + type_embed_dim, hidden_dim)

    def forward(self, unit_features: torch.Tensor, unit_types: torch.Tensor) -> torch.Tensor:
        clamped_types = unit_types.clamp(min=0, max=self.type_emb.num_embeddings - 1)
        type_h = self.type_emb(clamped_types)
        cont_h = self.cont_mlp(unit_features)
        return self.proj(torch.cat([cont_h, type_h], dim=-1))


class WorldModelNet(nn.Module):
    def __init__(
        self,
        hidden_dim: int = 96,
        cond_hidden_dim: int = 32,
        horizon_vocab_size: int = 5,
        terminal_self_master_mode: str = "none",
    ):
        super().__init__()
        self.terminal_self_master_mode = str(terminal_self_master_mode)
        self.unit_encoder = UnitEncoder(hidden_dim=hidden_dim)
        self.red_tactic_encoder = nn.Sequential(
            nn.Linear(TACTIC_FEATURE_DIM, cond_hidden_dim),
            nn.ReLU(),
            nn.Linear(cond_hidden_dim, cond_hidden_dim),
            nn.ReLU(),
        )
        self.blue_tactic_encoder = nn.Sequential(
            nn.Linear(TACTIC_FEATURE_DIM, cond_hidden_dim),
            nn.ReLU(),
            nn.Linear(cond_hidden_dim, cond_hidden_dim),
            nn.ReLU(),
        )
        self.horizon_emb = nn.Embedding(horizon_vocab_size, cond_hidden_dim)

        trunk_in = hidden_dim * 2 + cond_hidden_dim * 3 + 2
        self.trunk = nn.Sequential(
            nn.Linear(trunk_in, hidden_dim * 2),
            nn.ReLU(),
            nn.Linear(hidden_dim * 2, hidden_dim),
            nn.ReLU(),
        )

        traj_in = hidden_dim + hidden_dim
        self.red_alive_head = nn.Sequential(
            nn.Linear(traj_in, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
        )
        self.blue_alive_head = nn.Sequential(
            nn.Linear(traj_in, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 1),
        )
        self.red_reg_head = nn.Sequential(
            nn.Linear(traj_in, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, UNIT_FEATURE_DIM - 1),
        )
        self.blue_reg_head = nn.Sequential(
            nn.Linear(traj_in, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, UNIT_FEATURE_DIM - 1),
        )
        self.terminal_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, 5),
        )
        self.event_flag_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, EVENT_FLAG_DIM),
        )
        self.event_count_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, EVENT_COUNT_DIM),
        )
        self.reward_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, REWARD_DIM),
        )
        self.terminal_role_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, TERMINAL_CRITICAL_ROLE_DIM),
        )
        self.terminal_self_role_head = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, TERMINAL_SELF_ROLE_DIM),
        )

    def _build_joint_context(
        self,
        self_pool: torch.Tensor,
        other_pool: torch.Tensor,
        self_cond: torch.Tensor,
        other_cond: torch.Tensor,
        horizon_vec: torch.Tensor,
        self_count: torch.Tensor,
        other_count: torch.Tensor,
    ) -> torch.Tensor:
        return self.trunk(
            torch.cat([self_pool, other_pool, self_cond, other_cond, horizon_vec, self_count, other_count], dim=-1)
        )

    def forward(self, batch: Dict[str, torch.Tensor]) -> Dict[str, torch.Tensor]:
        red_entities = self.unit_encoder(batch["red_units"], batch["red_unit_types"])
        blue_entities = self.unit_encoder(batch["blue_units"], batch["blue_unit_types"])

        red_pool = masked_mean(red_entities, batch["red_mask"])
        blue_pool = masked_mean(blue_entities, batch["blue_mask"])
        red_cond = self.red_tactic_encoder(batch["red_tactic"])
        blue_cond = self.blue_tactic_encoder(batch["blue_tactic"])
        horizon_vec = self.horizon_emb(batch["horizon_id"])

        red_count = batch["red_mask"].sum(dim=1, keepdim=True).float()
        blue_count = batch["blue_mask"].sum(dim=1, keepdim=True).float()
        joint_context = self._build_joint_context(
            red_pool,
            blue_pool,
            red_cond,
            blue_cond,
            horizon_vec,
            red_count,
            blue_count,
        )
        blue_view_context = self._build_joint_context(
            blue_pool,
            red_pool,
            blue_cond,
            red_cond,
            horizon_vec,
            blue_count,
            red_count,
        )
        terminal_view_contexts = torch.stack([joint_context, blue_view_context], dim=1)

        expanded_context = joint_context.unsqueeze(1)
        red_traj_input = torch.cat([red_entities, expanded_context.expand(-1, red_entities.shape[1], -1)], dim=-1)
        blue_traj_input = torch.cat([blue_entities, expanded_context.expand(-1, blue_entities.shape[1], -1)], dim=-1)

        terminal_out = self.terminal_head(joint_context)
        event_flag_logits_raw = self.event_flag_head(joint_context)
        terminal_self_role_logits = self.terminal_self_role_head(terminal_view_contexts)
        event_flag_logits = event_flag_logits_raw
        if self.terminal_self_master_mode in {"self_derived", "self_derived_mirror_coupled"}:
            event_flag_logits = event_flag_logits_raw.clone()
            terminal_mask = batch["is_terminal_horizon"] > 0.5
            if bool(terminal_mask.any().item()):
                self_role_probs = torch.softmax(terminal_self_role_logits, dim=-1)
                red_self_first_kill_prob = self_role_probs[:, 0, SELF_FIRST_KILL_ROLE_INDEX]
                blue_self_first_kill_prob = self_role_probs[:, 1, SELF_FIRST_KILL_ROLE_INDEX]
                if self.terminal_self_master_mode == "self_derived_mirror_coupled":
                    # R1 mirror closure: keep the already-stable blue-facing path unchanged,
                    # and prevent terminal red first-kill from exceeding the mirrored blue gate.
                    red_self_first_kill_prob = torch.minimum(
                        red_self_first_kill_prob,
                        blue_self_first_kill_prob,
                    )
                red_self_first_kill_logit = torch.logit(
                    red_self_first_kill_prob.clamp(
                        min=DERIVED_LOGIT_EPS,
                        max=1.0 - DERIVED_LOGIT_EPS,
                    )
                )
                blue_self_first_kill_logit = torch.logit(
                    blue_self_first_kill_prob.clamp(
                        min=DERIVED_LOGIT_EPS,
                        max=1.0 - DERIVED_LOGIT_EPS,
                    )
                )
                event_flag_logits[terminal_mask, RED_FIRST_KILL_EVENT_INDEX] = red_self_first_kill_logit[terminal_mask]
                event_flag_logits[terminal_mask, BLUE_FIRST_KILL_EVENT_INDEX] = blue_self_first_kill_logit[terminal_mask]
        elif self.terminal_self_master_mode not in {"none", "self_head_only"}:
            raise ValueError(f"Unsupported terminal self-master mode: {self.terminal_self_master_mode}")

        return {
            "red_alive_logit": self.red_alive_head(red_traj_input).squeeze(-1),
            "blue_alive_logit": self.blue_alive_head(blue_traj_input).squeeze(-1),
            "red_traj_reg": self.red_reg_head(red_traj_input),
            "blue_traj_reg": self.blue_reg_head(blue_traj_input),
            "red_win_logit": terminal_out[:, 0],
            "terminal_red_alive_ratio": terminal_out[:, 1],
            "terminal_blue_alive_ratio": terminal_out[:, 2],
            "terminal_red_mean_missile": terminal_out[:, 3],
            "terminal_blue_mean_missile": terminal_out[:, 4],
            "event_flag_logits_raw": event_flag_logits_raw,
            "event_flag_logits": event_flag_logits,
            "event_count_pred": self.event_count_head(joint_context),
            "reward_pred": self.reward_head(joint_context),
            "terminal_critical_role_logits": self.terminal_role_head(joint_context),
            "terminal_self_role_logits": terminal_self_role_logits,
            "terminal_self_master_active": self.terminal_self_master_mode != "none",
        }
