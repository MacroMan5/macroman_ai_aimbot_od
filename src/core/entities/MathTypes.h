#pragma once

#include <cmath>
#include <cstdint>

namespace macroman {

/**
 * @brief 2D vector for positions and velocities
 *
 * Aligned to 8 bytes for potential SIMD usage.
 */
struct alignas(8) Vec2 {
    float x{0.0f};
    float y{0.0f};

    // Arithmetic operators
    Vec2 operator+(const Vec2& other) const noexcept {
        return {x + other.x, y + other.y};
    }

    Vec2 operator-(const Vec2& other) const noexcept {
        return {x - other.x, y - other.y};
    }

    Vec2 operator*(float scalar) const noexcept {
        return {x * scalar, y * scalar};
    }

    Vec2 operator/(float scalar) const noexcept {
        return {x / scalar, y / scalar};
    }

    Vec2& operator+=(const Vec2& other) noexcept {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vec2& operator*=(float scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    // Magnitude
    [[nodiscard]] float length() const noexcept {
        return std::sqrt(x * x + y * y);
    }

    [[nodiscard]] float lengthSquared() const noexcept {
        return x * x + y * y;
    }

    // Normalization
    [[nodiscard]] Vec2 normalized() const noexcept {
        float len = length();
        if (len < 1e-6f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }

    // Static utilities
    [[nodiscard]] static float distance(const Vec2& a, const Vec2& b) noexcept {
        return (b - a).length();
    }

    [[nodiscard]] static float distanceSquared(const Vec2& a, const Vec2& b) noexcept {
        return (b - a).lengthSquared();
    }

    [[nodiscard]] static float dot(const Vec2& a, const Vec2& b) noexcept {
        return a.x * b.x + a.y * b.y;
    }
};

/**
 * @brief Unique target identifier (monotonically increasing)
 */
struct TargetID {
    uint64_t value{0};

    bool operator==(const TargetID& other) const noexcept {
        return value == other.value;
    }

    bool operator!=(const TargetID& other) const noexcept {
        return value != other.value;
    }

    [[nodiscard]] bool isValid() const noexcept {
        return value != 0;
    }

    // For std::unordered_map
    struct Hash {
        size_t operator()(const TargetID& id) const noexcept {
            return std::hash<uint64_t>{}(id.value);
        }
    };
};

} // namespace macroman
