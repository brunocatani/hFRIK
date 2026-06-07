#pragma once

#include <cstdint>

namespace frik::hand_pose_stack_policy
{
    /*
     * Local transform overrides are an extension of a tagged scalar/per-bone
     * hand pose, not a standalone pose source. A transform-only entry makes the
     * hand-pose stack look active while leaving most bones to FRIK's fallback
     * controller path, which suppresses vanilla weapon poses without a complete
     * replacement. Keep the rule explicit so API callers cannot create no-op
     * active overrides by submitting an unknown tag or an empty mask.
     */
    enum class LocalTransformUpdateAction : std::uint8_t
    {
        RejectMissingScalarPose,
        ApplyToScalarPose,
        EraseTransformOnlyPose,
    };

    [[nodiscard]] constexpr LocalTransformUpdateAction classifyLocalTransformUpdate(
        bool tagExists,
        bool existingEntryHasScalarPose,
        std::uint16_t enabledMask)
    {
        const std::uint16_t sanitizedMask = enabledMask & 0x7FFF;
        if (!tagExists) {
            return LocalTransformUpdateAction::RejectMissingScalarPose;
        }

        if (existingEntryHasScalarPose) {
            return LocalTransformUpdateAction::ApplyToScalarPose;
        }

        if (sanitizedMask == 0) {
            return LocalTransformUpdateAction::EraseTransformOnlyPose;
        }

        return LocalTransformUpdateAction::RejectMissingScalarPose;
    }

    [[nodiscard]] constexpr bool sortsBefore(
        const int lhsPriority,
        const std::uint64_t lhsSequence,
        const int rhsPriority,
        const std::uint64_t rhsSequence)
    {
        if (lhsPriority != rhsPriority) {
            return lhsPriority > rhsPriority;
        }
        return lhsSequence > rhsSequence;
    }
}
