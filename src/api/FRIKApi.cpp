#define FRIK_API_EXPORTS
#include "FRIKApi.h"

#include "FRIKApiTagPolicy.h"
#include "FRIK.h"
#include "f4vr/F4VRSkelly.h"
#include "f4vr/F4VRUtils.h"
#include "f4vr/PlayerNodes.h"
#include "skeleton/HandPose.h"
#include "skeleton/Skeleton.h"

#include <array>
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    using namespace frik;
    using namespace frik::api;

    /**
     * Used to keep track of external tags blocking offhand gripping to prevent conflicts between client mods.
     * The actual tag values are not relevant to FRIK, only the fact that there is at least one tag blocking it.
     */
    std::unordered_set<std::string> g_offHandGripBlockingTags;

    struct ExternalHandAuthorityEntry
    {
        std::string tag;
        RE::NiTransform worldTarget;
        int priority = 0;
        std::uint64_t generation = 0;
    };

    struct SelectedExternalHandAuthority
    {
        bool valid = false;
        std::string tag;
        RE::NiTransform worldTarget;
    };

    std::array<std::vector<ExternalHandAuthorityEntry>, 2> g_externalHandAuthorities;
    std::uint64_t g_externalHandAuthorityGeneration = 0;
    std::mutex g_externalHandAuthorityLock;

    std::size_t handAuthorityIndex(bool isLeft) { return isLeft ? 1U : 0U; }

    SelectedExternalHandAuthority selectExternalHandAuthorityLocked(const std::vector<ExternalHandAuthorityEntry>& entries)
    {
        const ExternalHandAuthorityEntry* best = nullptr;
        for (const auto& entry : entries) {
            if (!best || entry.priority > best->priority || (entry.priority == best->priority && entry.generation > best->generation)) {
                best = &entry;
            }
        }

        if (!best) {
            return {};
        }
        return SelectedExternalHandAuthority{ .valid = true, .tag = best->tag, .worldTarget = best->worldTarget };
    }

    bool applyExternalHandAuthorityTransform(bool isLeft, const RE::NiTransform& worldTarget)
    {
        auto* skelly = g_frik.getSkeleton();
        if (!skelly) {
            return false;
        }

        const bool applied = skelly->applyExternalHandWorldTransform(isLeft, worldTarget);
        if (applied) {
            g_frik.refreshAfterExternalHandAuthority(isLeft);
            g_frik.syncPipboyAfterExternalHandAuthority(isLeft);
        }
        return applied;
    }

    bool restoreTrackedHandAuthorityTransform(bool isLeft)
    {
        auto* skelly = g_frik.getSkeleton();
        if (!skelly) {
            return false;
        }

        const bool restored = skelly->restoreTrackedHandAfterExternalAuthority(isLeft);
        if (restored) {
            g_frik.refreshAfterExternalHandAuthority(isLeft);
            g_frik.syncPipboyAfterExternalHandAuthority(isLeft);
        }
        return restored;
    }

    bool getIsLeftForHandEnum(const FRIKApi::Hand hand)
    {
        switch (hand) {
        case FRIKApi::Hand::Primary:
            return f4vr::isLeftHandedMode();
        case FRIKApi::Hand::Offhand:
            return !f4vr::isLeftHandedMode();
        case FRIKApi::Hand::Right:
            return false;
        case FRIKApi::Hand::Left:
            return true;
        }
        return false;
    }

    std::uint32_t FRIK_CALL getVersion()
    {
        return FRIK_API_VERSION;
    }

    const char* FRIK_CALL getModVersion()
    {
        // Safe to return pointer to static data
        static_assert(version::NAME.back() != '\0' || true, "Version must be backed by a string literal");
        return version::NAME.data();
    }

    bool FRIK_CALL isSkeletonReady()
    {
        return g_frik.isSkeletonReady();
    }

    bool FRIK_CALL isConfigOpen()
    {
        return g_frik.isMainConfigurationModeActive() || g_frik.isPipboyConfigurationModeActive() || g_frik.inWeaponRepositionMode();
    }

    bool FRIK_CALL isSelfieModeOn()
    {
        return g_frik.isSelfieModeOn();
    }

    void FRIK_CALL setSelfieModeOn(const bool setOn)
    {
        g_frik.setSelfieMode(setOn);
    }

    bool FRIK_CALL isOffHandGrippingWeapon()
    {
        return g_frik.isOffHandGrippingWeapon();
    }

    /**
     * Enable/disable FRIK offhand weapon gripping for a specific external tag.
     * Offhand gripping remains disabled while at least one tag is still blocking it.
     */
    bool FRIK_CALL blockOffHandWeaponGripping(const char* tag, const bool block)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) {
            return false;
        }

        if (block) {
            g_offHandGripBlockingTags.emplace(*normalizedTag);
        } else {
            g_offHandGripBlockingTags.erase(*normalizedTag);
        }

        logger::debug("API blockOffHandWeaponGripping tag:'{}' block:{} activeBlocks:{}", *normalizedTag, block, g_offHandGripBlockingTags.size());
        g_frik.setOffHandGrippingEnabled(g_offHandGripBlockingTags.empty());
        return true;
    }

    bool FRIK_CALL isWristPipboyOpen()
    {
        return g_frik.isPipboyOn();
    }

    RE::NiPoint3 FRIK_CALL getIndexFingerTipPosition(const FRIKApi::Hand hand)
    {
        return f4vr::Skelly::getIndexFingerTipWorldPosition(static_cast<vrcf::Hand>(hand));
    }

    FRIKApi::HandPoseTagState FRIK_CALL getHandPoseSetTagState(const char* tag, const FRIKApi::Hand hand)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) return FRIKApi::HandPoseTagState::None;
        return g_handPoseManager.getTagState(*normalizedTag, getIsLeftForHandEnum(hand));
    }

    FRIKApi::HandPoseKind FRIK_CALL getCurrentHandPose(const FRIKApi::Hand hand)
    {
        return g_handPoseManager.getCurrentPose(getIsLeftForHandEnum(hand));
    }

    bool FRIK_CALL setHandPose(const char* tag, const FRIKApi::Hand hand, FRIKApi::HandPoseKind handPose)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) return false;
        return g_handPoseManager.setTaggedPose(*normalizedTag, getIsLeftForHandEnum(hand), HandPosePriority::External, handPose);
    }

    bool FRIK_CALL setHandPoseCustomFingerPositions(const char* tag, const FRIKApi::Hand hand, const float thumb, const float index, const float middle, const float ring,
        const float pinky)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) return false;
        return g_handPoseManager.setTaggedPose(*normalizedTag, getIsLeftForHandEnum(hand), HandPosePriority::External,
            FRIKApi::HandPoseKind::Custom, thumb, index, middle, ring, pinky);
    }

    bool FRIK_CALL clearHandPose(const char* tag, const FRIKApi::Hand hand)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) return false;
        return g_handPoseManager.clearTaggedPose(*normalizedTag, getIsLeftForHandEnum(hand));
    }

    void FRIK_CALL setHandPoseFingerPositions(const FRIKApi::Hand hand, const float thumb, const float index, const float middle, const float ring, const float pinky)
    {
        // Deprecated path -- route through tag system with a legacy tag at low priority
        g_handPoseManager.setTaggedPose("_deprecated_api", getIsLeftForHandEnum(hand), HandPosePriority::Default,
            FRIKApi::HandPoseKind::Custom, thumb, index, middle, ring, pinky);
    }

    void FRIK_CALL clearHandPoseFingerPositions(const FRIKApi::Hand hand)
    {
        // Deprecated path -- clear the legacy tag
        g_handPoseManager.clearTaggedPose("_deprecated_api", getIsLeftForHandEnum(hand));
    }

    bool FRIK_CALL registerOpenModSettingButtonToMainConfig(const FRIKApi::OpenExternalModConfigData& data)
    {
        if (!data.buttonIconNifPath || !data.callbackReceiverName) {
            return false;
        }
        g_frik.registerOpenSettingButton({
            .buttonIconNifPath = data.buttonIconNifPath,
            .callbackReceiverName = data.callbackReceiverName,
            .callbackMessageType = data.callbackMessageType
        });
        return true;
    }

    // =====================================================================
    // Upstream v4 compatibility: full hand pose data.
    // =====================================================================

    bool FRIK_CALL setHandPoseCustom(const char* tag, const FRIKApi::Hand hand, const FRIKApi::HandPoseData& handPose, const bool forceTop)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) return false;
        return g_handPoseManager.setTaggedPoseCustom(
            *normalizedTag,
            getIsLeftForHandEnum(hand),
            forceTop ? HandPosePriority::ForcedExternal : HandPosePriority::External,
            handPose);
    }

    // =====================================================================
    // API v5: ROCK visual-authority additions kept in the base FRIKApi table.
    // =====================================================================

    bool FRIK_CALL setHandPoseWithPriority(const char* tag, const FRIKApi::Hand hand, FRIKApi::HandPoseKind handPose, int priority)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) return false;
        return g_handPoseManager.setTaggedPose(*normalizedTag, getIsLeftForHandEnum(hand), priority, handPose);
    }

    bool FRIK_CALL setHandPoseCustomFingerPositionsWithPriority(const char* tag, const FRIKApi::Hand hand, const float thumb, const float index, const float middle,
        const float ring, const float pinky, const int priority)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) return false;
        return g_handPoseManager.setTaggedPose(*normalizedTag, getIsLeftForHandEnum(hand), priority, FRIKApi::HandPoseKind::Custom, thumb, index, middle, ring, pinky);
    }

    bool FRIK_CALL setHandPoseCustomJointPositionsWithPriority(const char* tag, const FRIKApi::Hand hand, const float* values, const int priority)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag || !values) return false;
        return g_handPoseManager.setTaggedPosePerBone(*normalizedTag, getIsLeftForHandEnum(hand), priority, values);
    }

    bool FRIK_CALL setHandPoseCustomLocalTransformsWithPriority(
        const char* tag,
        const FRIKApi::Hand hand,
        const FRIKApi::FingerLocalTransformOverride* overrideData,
        const int priority)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag || !overrideData) return false;
        return g_handPoseManager.setTaggedPoseLocalTransforms(*normalizedTag, getIsLeftForHandEnum(hand), priority, *overrideData);
    }

    bool FRIK_CALL getHandPoseLocalTransformsForJointPositions(
        const FRIKApi::Hand hand,
        const float* values,
        FRIKApi::FingerLocalTransformOverride* outTransforms)
    {
        if (!values || !outTransforms) return false;
        return buildFingerLocalTransformsForJointPositions(getIsLeftForHandEnum(hand), values, *outTransforms);
    }

    RE::NiTransform FRIK_CALL apiGetHandWorldTransform(const FRIKApi::Hand hand)
    {
        auto* skelly = g_frik.getSkeleton();
        if (!skelly) return RE::NiTransform();
        return skelly->getHandWorldTransform(getIsLeftForHandEnum(hand));
    }

    bool FRIK_CALL apiApplyExternalHandWorldTransform(const char* tag, const FRIKApi::Hand hand, const RE::NiTransform& worldTarget, const int priority)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag || priority < 0) {
            return false;
        }

        if (!g_frik.getSkeleton()) {
            return false;
        }

        const bool isLeft = getIsLeftForHandEnum(hand);
        SelectedExternalHandAuthority selected{};
        {
            std::scoped_lock lock(g_externalHandAuthorityLock);
            auto& entries = g_externalHandAuthorities[handAuthorityIndex(isLeft)];
            auto it = std::find_if(entries.begin(), entries.end(), [&](const ExternalHandAuthorityEntry& entry) { return entry.tag == *normalizedTag; });
            if (it == entries.end()) {
                entries.push_back(ExternalHandAuthorityEntry{
                    .tag = *normalizedTag,
                    .worldTarget = worldTarget,
                    .priority = priority,
                    .generation = ++g_externalHandAuthorityGeneration,
                });
            } else {
                it->worldTarget = worldTarget;
                it->priority = priority;
                it->generation = ++g_externalHandAuthorityGeneration;
            }
            selected = selectExternalHandAuthorityLocked(entries);
        }

        if (!selected.valid) {
            return false;
        }

        return selected.tag == *normalizedTag ? applyExternalHandAuthorityTransform(isLeft, selected.worldTarget) : true;
    }

    bool FRIK_CALL apiClearExternalHandWorldTransform(const char* tag, const FRIKApi::Hand hand)
    {
        const auto normalizedTag = tag_policy::normalizeTag(tag);
        if (!normalizedTag) {
            return false;
        }

        const bool isLeft = getIsLeftForHandEnum(hand);
        SelectedExternalHandAuthority selected{};
        bool removed = false;
        {
            std::scoped_lock lock(g_externalHandAuthorityLock);
            auto& entries = g_externalHandAuthorities[handAuthorityIndex(isLeft)];
            const auto oldSize = entries.size();
            entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const ExternalHandAuthorityEntry& entry) { return entry.tag == *normalizedTag; }), entries.end());
            removed = entries.size() != oldSize;
            selected = selectExternalHandAuthorityLocked(entries);
        }

        if (!removed) {
            return true;
        }
        if (!selected.valid) {
            (void)restoreTrackedHandAuthorityTransform(isLeft);
            return true;
        }
        return applyExternalHandAuthorityTransform(isLeft, selected.worldTarget);
    }

    constexpr FRIKApi FRIK_API_FUNCTIONS_TABLE{
        .getVersion = &getVersion,
        .getModVersion = &getModVersion,
        .isSkeletonReady = &isSkeletonReady,
        .isConfigOpen = &isConfigOpen,
        .isSelfieModeOn = &isSelfieModeOn,
        .setSelfieModeOn = &setSelfieModeOn,
        .isOffHandGrippingWeapon = &isOffHandGrippingWeapon,
        .isWristPipboyOpen = &isWristPipboyOpen,
        .getIndexFingerTipPosition = &getIndexFingerTipPosition,
        .getHandPoseSetTagState = &getHandPoseSetTagState,
        .getCurrentHandPose = &getCurrentHandPose,
        .setHandPose = &setHandPose,
        .setHandPoseCustomFingerPositions = &setHandPoseCustomFingerPositions,
        .clearHandPose = &clearHandPose,
        .setHandPoseFingerPositions = &setHandPoseFingerPositions,
        .clearHandPoseFingerPositions = &clearHandPoseFingerPositions,
        .registerOpenModSettingButtonToMainConfig = &registerOpenModSettingButtonToMainConfig,
        .blockOffHandWeaponGripping = &blockOffHandWeaponGripping,
        .setHandPoseCustom = &setHandPoseCustom,
        .setHandPoseWithPriority = &setHandPoseWithPriority,
        .getHandWorldTransform = &apiGetHandWorldTransform,
        .setHandPoseCustomFingerPositionsWithPriority = &setHandPoseCustomFingerPositionsWithPriority,
        .setHandPoseCustomJointPositionsWithPriority = &setHandPoseCustomJointPositionsWithPriority,
        .applyExternalHandWorldTransform = &apiApplyExternalHandWorldTransform,
        .clearExternalHandWorldTransform = &apiClearExternalHandWorldTransform,
        .setHandPoseCustomLocalTransformsWithPriority = &setHandPoseCustomLocalTransformsWithPriority,
        .getHandPoseLocalTransformsForJointPositions = &getHandPoseLocalTransformsForJointPositions,
    };
}

namespace frik::api
{
    FRIK_API const FRIKApi* FRIK_CALL FRIKAPI_GetApi()
    {
        return &FRIK_API_FUNCTIONS_TABLE;
    }
}
