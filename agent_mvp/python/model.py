from typing import Dict

import torch
import torch.nn as nn


class UnitEncoder(nn.Module):
    def __init__(self, hidden_dim: int, num_type_buckets: int = 16, type_embed_dim: int = 8):
        super().__init__()
        self.type_emb = nn.Embedding(num_type_buckets, type_embed_dim)
        self.mlp = nn.Sequential(
            nn.Linear(5, hidden_dim),
            nn.ReLU(),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(),
        )
        self.proj = nn.Linear(hidden_dim + type_embed_dim, hidden_dim)

    def forward(self, units: torch.Tensor) -> torch.Tensor:
        # units: [B, N, 6] -> [type_id, speed, sensor, missile, alive, relative_distance]
        type_ids = units[..., 0].long().clamp(min=0, max=self.type_emb.num_embeddings - 1)
        cont = units[..., 1:]
        cont_h = self.mlp(cont)
        type_h = self.type_emb(type_ids)
        return self.proj(torch.cat([cont_h, type_h], dim=-1))


class SelfAttnBlock(nn.Module):
    def __init__(self, hidden_dim: int, num_heads: int = 4, dropout: float = 0.0):
        super().__init__()
        self.attn = nn.MultiheadAttention(
            embed_dim=hidden_dim,
            num_heads=num_heads,
            dropout=dropout,
            batch_first=True,
        )
        self.norm1 = nn.LayerNorm(hidden_dim)
        self.ffn = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim * 2),
            nn.ReLU(),
            nn.Linear(hidden_dim * 2, hidden_dim),
        )
        self.norm2 = nn.LayerNorm(hidden_dim)

    def forward(self, x: torch.Tensor, valid_mask: torch.Tensor) -> torch.Tensor:
        key_padding_mask = ~valid_mask
        attn_out, _ = self.attn(x, x, x, key_padding_mask=key_padding_mask, need_weights=False)
        x = self.norm1(x + attn_out)
        return self.norm2(x + self.ffn(x))


class CrossAttnBlock(nn.Module):
    def __init__(self, hidden_dim: int, num_heads: int = 4, dropout: float = 0.0):
        super().__init__()
        self.attn = nn.MultiheadAttention(
            embed_dim=hidden_dim,
            num_heads=num_heads,
            dropout=dropout,
            batch_first=True,
        )
        self.norm1 = nn.LayerNorm(hidden_dim)
        self.ffn = nn.Sequential(
            nn.Linear(hidden_dim, hidden_dim * 2),
            nn.ReLU(),
            nn.Linear(hidden_dim * 2, hidden_dim),
        )
        self.norm2 = nn.LayerNorm(hidden_dim)

    def forward(
        self,
        query_x: torch.Tensor,
        kv_x: torch.Tensor,
        kv_valid_mask: torch.Tensor,
    ) -> torch.Tensor:
        key_padding_mask = ~kv_valid_mask
        attn_out, _ = self.attn(
            query_x,
            kv_x,
            kv_x,
            key_padding_mask=key_padding_mask,
            need_weights=False,
        )
        x = self.norm1(query_x + attn_out)
        return self.norm2(x + self.ffn(x))


def masked_pool(x: torch.Tensor, mask: torch.Tensor) -> torch.Tensor:
    # x: [B, N, H], mask: [B, N]
    m = mask.unsqueeze(-1)
    count = m.sum(dim=1).clamp(min=1)
    mean = (x * m).sum(dim=1) / count

    neg_inf = torch.full_like(x, -1e9)
    x_masked = torch.where(m, x, neg_inf)
    maxv = x_masked.max(dim=1).values
    return torch.cat([mean, maxv], dim=-1)


class MacroEvalNet(nn.Module):
    def __init__(
        self,
        unit_hidden: int = 64,
        tactic_dim: int = 5,
        num_heads: int = 4,
    ):
        super().__init__()
        self.encoder = UnitEncoder(unit_hidden)
        self.red_self_attn = SelfAttnBlock(unit_hidden, num_heads=num_heads)
        self.blue_self_attn = SelfAttnBlock(unit_hidden, num_heads=num_heads)
        self.red_cross_attn = CrossAttnBlock(unit_hidden, num_heads=num_heads)
        self.blue_cross_attn = CrossAttnBlock(unit_hidden, num_heads=num_heads)
        self.tactic_net = nn.Sequential(nn.Linear(tactic_dim, 32), nn.ReLU())

        trunk_in = unit_hidden * 4 + 32 + 2
        self.trunk = nn.Sequential(
            nn.Linear(trunk_in, 128),
            nn.ReLU(),
            nn.Linear(128, 128),
            nn.ReLU(),
        )

        self.win_head = nn.Linear(128, 1)
        self.red_survival_head = nn.Sequential(
            nn.Linear(unit_hidden * 2, unit_hidden),
            nn.ReLU(),
            nn.Linear(unit_hidden, 1),
        )
        self.blue_survival_head = nn.Sequential(
            nn.Linear(unit_hidden * 2, unit_hidden),
            nn.ReLU(),
            nn.Linear(unit_hidden, 1),
        )
        self.red_missile_head = nn.Sequential(
            nn.Linear(unit_hidden * 2, unit_hidden),
            nn.ReLU(),
            nn.Linear(unit_hidden, 1),
            nn.ReLU(),
        )
        self.blue_missile_head = nn.Sequential(
            nn.Linear(unit_hidden * 2, unit_hidden),
            nn.ReLU(),
            nn.Linear(unit_hidden, 1),
            nn.ReLU(),
        )

    def forward(self, batch: Dict[str, torch.Tensor]) -> Dict[str, torch.Tensor]:
        red = self.encoder(batch["red_units"])
        blue = self.encoder(batch["blue_units"])

        # Self-attention captures internal coordination for each side.
        red_self = self.red_self_attn(red, batch["red_mask"])
        blue_self = self.blue_self_attn(blue, batch["blue_mask"])

        # Cross-attention captures interactive combat effects between two sides.
        red = self.red_cross_attn(red_self, blue_self, batch["blue_mask"])
        blue = self.blue_cross_attn(blue_self, red_self, batch["red_mask"])

        red_vec = masked_pool(red, batch["red_mask"])
        blue_vec = masked_pool(blue, batch["blue_mask"])
        tactic_vec = self.tactic_net(batch["tactic"])

        red_count = batch["red_mask"].sum(dim=1, keepdim=True).float()
        blue_count = batch["blue_mask"].sum(dim=1, keepdim=True).float()

        fused = torch.cat([red_vec, blue_vec, tactic_vec, red_count, blue_count], dim=-1)
        h = self.trunk(fused)

        red_entity_h = torch.cat([red_self, red], dim=-1)
        blue_entity_h = torch.cat([blue_self, blue], dim=-1)

        red_survival_probs = torch.sigmoid(self.red_survival_head(red_entity_h).squeeze(-1))
        blue_survival_probs = torch.sigmoid(self.blue_survival_head(blue_entity_h).squeeze(-1))

        red_final_missile = self.red_missile_head(red_entity_h).squeeze(-1)
        blue_final_missile = self.blue_missile_head(blue_entity_h).squeeze(-1)

        red_initial_missile = batch["red_units"][..., 3]
        blue_initial_missile = batch["blue_units"][..., 3]
        red_missile_expenditure = (red_initial_missile - red_final_missile).clamp(min=0.0)
        blue_missile_expenditure = (blue_initial_missile - blue_final_missile).clamp(min=0.0)

        return {
            "win_rate": torch.sigmoid(self.win_head(h).squeeze(-1)),
            "red_survival_probs": red_survival_probs,
            "blue_survival_probs": blue_survival_probs,
            "red_final_missile": red_final_missile,
            "blue_final_missile": blue_final_missile,
            "red_missile_expenditure": red_missile_expenditure,
            "blue_missile_expenditure": blue_missile_expenditure,
        }

