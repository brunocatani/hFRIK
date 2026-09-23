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

    inline bool hasOrientationCue(const float lowBlend, const float faceUp, const float rightUp, const float headwardHorizontalLength)
    {
        return std::isfinite(lowBlend) && std::isfinite(faceUp) && std::isfinite(rightUp) && std::isfinite(headwardHorizontalLength) &&
            lowBlend >= 0.5f && headwardHorizontalLength >= 0.55f && (faceUp > 0.6f || std::abs(rightUp) > 0.7f);
    }

    // The headset cannot distinguish a head turn from a torso roll. Allow 45 degrees of head movement
    // around belly/back before moving the body, with a smooth passage through either side.
    inline std::optional<float> bodyRoll(const float rightUp, const float rightAcross)
    {
        if (!std::isfinite(rightUp) || !std::isfinite(rightAcross) || std::hypot(rightUp, rightAcross) < 0.35f) {
            return std::nullopt;
        }
        const float angle = std::atan2(rightUp, rightAcross);
        const float t = std::clamp((std::abs(angle) - std::numbers::pi_v<float> * 0.25f) / (std::numbers::pi_v<float> * 0.5f), 0.0f, 1.0f);
        return std::copysign(t * t * (3.0f - 2.0f * t) * std::numbers::pi_v<float>, angle);
    }

    struct FloorFrame
    {
        std::array<float, 3> right;
        std::array<float, 3> front;
        std::array<float, 3> headward;
    };

    inline std::optional<FloorFrame> floorFrame(const float headwardX, const float headwardY, const float roll)
    {
        const float length = std::hypot(headwardX, headwardY);
        if (!std::isfinite(length) || length < 0.0001f || !std::isfinite(roll)) {
            return std::nullopt;
        }
        const float x = headwardX / length;
        const float y = headwardY / length;
        const float c = std::cos(roll);
        const float s = std::sin(roll);
        return FloorFrame{ { y * c, -x * c, s }, { y * s, -x * s, -c }, { x, y, 0.0f } };
    }

    inline float orientationMoveFraction(const float seconds)
    {
        // Time constant for the visible torso transition; hand tracking is not smoothed here.
        return std::isfinite(seconds) && seconds > 0.0f ? -std::expm1(-seconds / 0.15f) : 0.0f;
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
