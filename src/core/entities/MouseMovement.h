#pragma once

#include <cmath>

namespace sunone {

struct MouseMovement {
    int dx;
    int dy;
    float duration;  // Suggested duration in ms

    MouseMovement() : dx(0), dy(0), duration(0) {}
    MouseMovement(int x, int y, float dur = 0) : dx(x), dy(y), duration(dur) {}

    bool isZero() const { return dx == 0 && dy == 0; }

    float magnitude() const {
        return std::sqrt(static_cast<float>(dx * dx + dy * dy));
    }
};

} // namespace sunone
