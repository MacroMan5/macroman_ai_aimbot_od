#include "TargetSelector.h"
#include <cmath>
#include <limits>

namespace macroman {

std::optional<TargetSelection> TargetSelector::selectBest(
    const TargetDatabase& db,
    Vec2 crosshair,
    float fovRadius
) const noexcept {
    if (db.count == 0) {
        return std::nullopt;
    }

    std::optional<TargetSelection> best;
    int bestPriority = -1;
    float bestDistance = std::numeric_limits<float>::max();

    for (size_t i = 0; i < db.count; ++i) {
        // Calculate distance from crosshair
        Vec2 pos = db.positions[i];
        float dx = pos.x - crosshair.x;
        float dy = pos.y - crosshair.y;
        float distance = std::sqrt(dx * dx + dy * dy);

        // Filter by FOV
        if (distance > fovRadius) {
            continue;  // Outside FOV
        }

        // Get hitbox priority
        int priority = getHitboxPriority(db.hitboxTypes[i]);

        // Select if:
        // 1. Higher priority hitbox, OR
        // 2. Same priority but closer
        bool isBetter = false;
        if (priority > bestPriority) {
            isBetter = true;
        } else if (priority == bestPriority && distance < bestDistance) {
            isBetter = true;
        }

        if (isBetter) {
            best = TargetSelection{
                i,
                pos,
                db.hitboxTypes[i],
                distance
            };
            bestPriority = priority;
            bestDistance = distance;
        }
    }

    return best;
}

} // namespace macroman
