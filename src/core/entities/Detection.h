#pragma once

#include <cstdint>

namespace macroman {

/**
 * @brief Bounding box in pixel coordinates
 */
struct BBox {
    float x;        // Top-left X
    float y;        // Top-left Y
    float width;    // Width
    float height;   // Height

    float area() const {
        return width * height;
    }
};

/**
 * @brief Hitbox type classification
 */
enum class HitboxType : uint8_t {
    Unknown = 0,
    Head = 1,
    Chest = 2,
    Body = 3
};

/**
 * @brief Single detection from YOLO model
 */
struct Detection {
    BBox bbox;          // Bounding box
    float confidence;   // Detection confidence [0.0, 1.0]
    int classId;        // Class ID from model
    HitboxType hitbox;  // Mapped hitbox type

    Detection()
        : bbox{0, 0, 0, 0}
        , confidence(0.0f)
        , classId(0)
        , hitbox(HitboxType::Unknown)
    {}
};

} // namespace macroman
