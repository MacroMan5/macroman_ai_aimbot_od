#pragma once

#include <optional>
#include "core/entities/TargetDatabase.h"

namespace macroman {

/**
 * @brief Result of target selection
 */
struct TargetSelection {
    size_t targetIndex;       // Index in TargetDatabase
    Vec2 position;            // Current position
    HitboxType hitbox;        // Hitbox type for aim priority
    float distanceToCenter;   // Distance from crosshair (for debugging)
};

/**
 * @brief Selects the best target to aim at from TargetDatabase
 *
 * Selection criteria (in priority order):
 * 1. Within FOV (circular region around crosshair)
 * 2. Hitbox priority: Head > Chest > Body
 * 3. Distance to crosshair (closer is better)
 */
class TargetSelector {
public:
    /**
     * @brief Select the best target from database
     * @param db Target database (SoA layout)
     * @param crosshair Crosshair position (screen center)
     * @param fovRadius FOV radius in pixels (circular region)
     * @return Selected target, or nullopt if no valid targets
     */
    [[nodiscard]] std::optional<TargetSelection> selectBest(
        const TargetDatabase& db,
        Vec2 crosshair,
        float fovRadius
    ) const noexcept;

private:
    // Hitbox priority weights (higher = more important)
    static constexpr int getHitboxPriority(HitboxType hitbox) noexcept {
        switch (hitbox) {
            case HitboxType::Head:  return 3;
            case HitboxType::Chest: return 2;
            case HitboxType::Body:  return 1;
            default:                return 0;
        }
    }
};

} // namespace macroman
