#pragma once

#include "Skeleton.h"
#include "api/FRIKApi.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace frik
{
    extern std::map<std::string, RE::NiTransform, common::CaseInsensitiveComparator> handClosed;
    extern std::map<std::string, RE::NiTransform, common::CaseInsensitiveComparator> handOpen;

    extern std::map<std::string, float> handPapyrusPose;
    extern std::map<std::string, bool> handPapyrusHasControl;
    extern std::map<std::string, RE::NiTransform, common::CaseInsensitiveComparator> handLocalTransformOverride;
    extern std::map<std::string, bool> handLocalTransformHasControl;
    extern std::map<std::string, float, common::CaseInsensitiveComparator> handPoseSplay;
    extern std::map<std::string, bool, common::CaseInsensitiveComparator> handPoseSplayHasControl;
    extern std::array<float, 2> handPosePalmPitch;
    extern std::array<float, 2> handPosePalmYaw;
    extern std::array<bool, 2> handPosePalmHasControl;

    void initHandPoses(bool inPowerArmor);

    float getHandBonePose(const std::string& bone, const bool melee);

    void setFingerPositionScalar(bool isLeft, float thumb, float index, float middle, float ring, float pinky);
    void setFingerJointPositions(bool isLeft, const float values[15]);
    void restoreFingerPoseControl(bool isLeft);
    bool buildFingerLocalTransformsForJointPositions(bool isLeft, const float values[15], api::FRIKApi::FingerLocalTransformOverride& outTransforms);

    void setPipboyHandPose();
    void disablePipboyHandPose();
    void setConfigModeHandPose();
    void disableConfigModePose();

    void setForceHandPointingPose(bool primaryHand, bool forcePointing);

    void setOffhandGripHandPose(bool toSet);
    void setAttaboyHandPose(bool toSet);
    void setHandPoseOverride(bool override, bool rightHand, const float* handPose);

    // --- Tag-based hand pose priority system ---

    /// Known priority levels for internal and external systems.
    /// Higher value = higher priority (wins when multiple tags are active).
    namespace HandPosePriority
    {
        constexpr int Default = 0;         // Fallback / no override
        constexpr int External = 50;       // External mods via API (default)
        constexpr int ForcedExternal = 90; // Upstream compatibility forceTop=true, still below ROCK grab
        constexpr int OffhandGrip = 60;    // FRIK offhand weapon grip
        constexpr int Pipboy = 70;         // Pipboy pointing pose
        constexpr int ConfigMode = 70;     // Config mode pointing pose
        constexpr int Attaboy = 65;        // Attaboy pose
        constexpr int PhysicsGrab = 100;   // ROCK physics grab
    }

    /// A single hand pose override entry in the priority stack.
    struct HandPoseEntry
    {
        std::string tag;                           // Unique identifier (e.g., "ROCK_Grab", "Pipboy")
        int priority = HandPosePriority::Default;  // Higher wins
        std::uint64_t sequence = 0;                // Newer same-priority entries win
        api::FRIKApi::HandPoseKind poseType = api::FRIKApi::HandPoseKind::Custom;
        api::FRIKApi::HandPoseData poseData = {};
        float fingers[5] = { 0, 0, 0, 0, 0 };    // thumb, index, middle, ring, pinky (0=bent, 1=straight)
        float perBoneFingers[15] = {};             // Optional per-bone override (if set from raw float[15])
        bool usesPerBone = false;                  // true = use perBoneFingers[15], false = use fingers[5]
        bool usesFullPoseData = false;             // true = poseData carries caller-authored splay/palm data
        bool hasScalarPose = true;                 // true = perBoneFingers controls the scalar open/closed blend
        std::uint16_t localTransformMask = 0;       // Bit i enables localTransforms[i]
        RE::NiTransform localTransforms[15] = {};   // Optional local transform overrides for selected finger bones
    };

    /// Manages a priority stack of hand pose overrides per hand.
    /// Replaces the old global handPapyrusPose/handPapyrusHasControl approach.
    class HandPoseManager
    {
    public:
        /// Set or update a tagged hand pose with 5-finger scalars.
        bool setTaggedPose(const std::string& tag, bool isLeft, int priority,
            api::FRIKApi::HandPoseKind poseType, float thumb, float index, float middle, float ring, float pinky);

        /// Set or update a tagged hand pose with a predefined pose enum.
        bool setTaggedPose(const std::string& tag, bool isLeft, int priority, api::FRIKApi::HandPoseKind poseType);

        /// Set or update a tagged full hand pose with per-joint curls, splay, and palm offsets.
        bool setTaggedPoseCustom(const std::string& tag, bool isLeft, int priority,
            const api::FRIKApi::HandPoseData& poseData,
            api::FRIKApi::HandPoseKind poseType = api::FRIKApi::HandPoseKind::Custom);

        /// Set or update a tagged hand pose with raw per-bone float[15] array (internal use).
        bool setTaggedPosePerBone(const std::string& tag, bool isLeft, int priority, const float* perBoneValues);

        /// Set or update local finger-bone transform overrides for a tagged hand pose.
        bool setTaggedPoseLocalTransforms(const std::string& tag, bool isLeft, int priority, const api::FRIKApi::FingerLocalTransformOverride& overrideData);

        /// Remove a tagged hand pose override.
        bool clearTaggedPose(const std::string& tag, bool isLeft);

        /// Get the state of a specific tag.
        api::FRIKApi::HandPoseTagState getTagState(const std::string& tag, bool isLeft) const;

        /// Get the currently active (top priority) pose type.
        api::FRIKApi::HandPoseKind getCurrentPose(bool isLeft) const;

        /// Check if any override is active for a hand.
        bool hasActiveOverride(bool isLeft) const;

    private:
        /// Apply the top-priority entry to the legacy global maps (handPapyrusPose/handPapyrusHasControl).
        void applyTopEntry(bool isLeft);

        /// Clear the legacy global maps for a hand.
        void clearLegacyMaps(bool isLeft);

        /// Convert a predefined pose enum to per-bone float[15] values.
        static const api::FRIKApi::HandPoseData* getPredefinedPoseData(api::FRIKApi::HandPoseKind pose);

        // Per-hand stacks: [0] = right, [1] = left. Sorted by priority descending.
        std::vector<HandPoseEntry> _stacks[2];
        std::uint64_t _nextSequence = 0;
    };

    /// Global hand pose manager instance.
    inline HandPoseManager g_handPoseManager;
}
