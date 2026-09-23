#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace frik::lowposture
{
    // Physical, floor-relative headset height, independent of comfort sneak and camera offsets.
    // The lower band represents a stomach-down pose; the upper band joins the existing crouch.
    inline std::optional<float> blendFromHeight(const float heightMeters, const float standingHeightMeters)
    {
        if (!std::isfinite(heightMeters) || !std::isfinite(standingHeightMeters) || heightMeters < 0.0f || standingHeightMeters <= 0.0f) {
            return std::nullopt;
        }
        const float t = std::clamp((0.40f - heightMeters / standingHeightMeters) / (0.40f - 0.22f), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    inline float blendHeading(const float live, const float held, const float blend)
    {
        return live + std::remainder(held - live, 2.0f * std::numbers::pi_v<float>) * blend;
    }

    // Equivalent to normalizing (hip - neck + backward * tan(pitch) * distance) below 90 degrees,
    // but remains continuous at horizontal. No huge intermediate hip position or tangent sign flip.
    inline std::optional<std::array<float, 3>> hipDirection(
        const std::array<float, 3>& neckToHip, const std::array<float, 3>& backward, const float pitch)
    {
        const float distance = std::hypot(neckToHip[0], neckToHip[1], neckToHip[2]);
        const float backwardLength = std::hypot(backward[0], backward[1], backward[2]);
        if (!std::isfinite(distance) || !std::isfinite(backwardLength) || !std::isfinite(pitch) || distance < 0.0001f || backwardLength < 0.0001f) {
            return std::nullopt;
        }
        std::array<float, 3> direction{};
        const float cosine = std::cos(pitch);
        const float sineDistance = distance * std::sin(pitch);
        for (std::size_t i = 0; i < direction.size(); ++i) {
            direction[i] = neckToHip[i] * cosine + backward[i] / backwardLength * sineDistance;
        }
        const float length = std::hypot(direction[0], direction[1], direction[2]);
        if (!std::isfinite(length) || length < 0.0001f) {
            return std::nullopt;
        }
        for (float& value : direction) {
            value /= length;
        }
        return direction;
    }
}
