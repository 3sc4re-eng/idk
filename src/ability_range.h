#pragma once
#include "entities.h"
#include <algorithm>
#include <cmath>

namespace ability_range {
// Installed 2026-10-31 item definitions: Greater Expansion gives 30% global
// range/radius; Mystic Expansion gives 20% only to its imbued ability.
// Use the strongest known bonus as a conservative lower bound. Other live
// modifiers and stacking rules are not inferred from a static asset table.
inline float known_lower_bound(const Entity& local, u32 ability, float base) {
    if (!std::isfinite(base) || base <= 0 || !local.abilities_valid) return base;
    float bonus = 0;
    for (const auto& item : local.abilities) {
        if (item.token == 1193964439u) bonus = std::max(bonus, .30f); // Greater Expansion
        if (item.token == 754480263u && item.imbued_valid &&
            std::find(item.imbued_tokens.begin(), item.imbued_tokens.end(), ability) != item.imbued_tokens.end())
            bonus = std::max(bonus, .20f); // Mystic Expansion
    }
    return base * (1 + bonus);
}
}
