#pragma once

/*
 * External callers need FRIK's authored finger transform baseline before they
 * can add mesh-contact corrections. Keeping the bone order, value sanitation,
 * and request validity here makes the API contract explicit: FRIK converts its
 * own 15 scalar joint values to local transforms, while callers only decide how
 * to adjust those transforms for their interaction surface.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace frik::hand_pose_local_transform_policy
{
    inline constexpr std::size_t kFingerLocalTransformCount = 15;
    inline constexpr std::uint16_t kFullFingerLocalTransformMask = 0x7FFF;

    [[nodiscard]] inline bool shouldBuildBaselineLocalTransforms(const float* values)
    {
        return values != nullptr;
    }

    [[nodiscard]] inline float sanitizePoseBlendValue(float value)
    {
        if (!std::isfinite(value)) {
            return 1.0f;
        }
        return std::clamp(value, -1.0f, 2.0f);
    }

    [[nodiscard]] inline const char* fingerLocalTransformBoneName(bool isLeft, std::size_t index)
    {
        static constexpr std::array<const char*, kFingerLocalTransformCount> kRightNames{
            "RArm_Finger11",
            "RArm_Finger12",
            "RArm_Finger13",
            "RArm_Finger21",
            "RArm_Finger22",
            "RArm_Finger23",
            "RArm_Finger31",
            "RArm_Finger32",
            "RArm_Finger33",
            "RArm_Finger41",
            "RArm_Finger42",
            "RArm_Finger43",
            "RArm_Finger51",
            "RArm_Finger52",
            "RArm_Finger53",
        };
        static constexpr std::array<const char*, kFingerLocalTransformCount> kLeftNames{
            "LArm_Finger11",
            "LArm_Finger12",
            "LArm_Finger13",
            "LArm_Finger21",
            "LArm_Finger22",
            "LArm_Finger23",
            "LArm_Finger31",
            "LArm_Finger32",
            "LArm_Finger33",
            "LArm_Finger41",
            "LArm_Finger42",
            "LArm_Finger43",
            "LArm_Finger51",
            "LArm_Finger52",
            "LArm_Finger53",
        };

        if (index >= kFingerLocalTransformCount) {
            return nullptr;
        }
        return isLeft ? kLeftNames[index] : kRightNames[index];
    }
}
