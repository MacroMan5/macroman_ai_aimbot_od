#pragma once

#include "MathTypes.h"
#include "Detection.h"  // For HitboxType

namespace macroman {

/**
 * @brief Command from Tracking thread to Input thread
 *
 * This POD structure is passed via atomic store/load.
 * It contains the predicted target position for the Input thread to aim at.
 *
 * Memory layout: 20 bytes (4+8+4+4)
 * - bool hasTarget (4 bytes with padding)
 * - Vec2 targetPosition (8 bytes)
 * - float confidence (4 bytes)
 * - HitboxType hitbox (4 bytes)
 */
struct AimCommand {
    bool hasTarget{false};           // True if valid target exists
    Vec2 targetPosition{0.0f, 0.0f}; // Predicted screen position (pixels)
    float confidence{0.0f};          // Detection confidence (0.0-1.0)
    HitboxType hitbox{HitboxType::Body}; // Target hitbox type

    // Default constructor (zero-initialized)
    AimCommand() = default;

    // Construct with target
    AimCommand(Vec2 pos, float conf, HitboxType hb)
        : hasTarget(true)
        , targetPosition(pos)
        , confidence(conf)
        , hitbox(hb) {}
};

} // namespace macroman
