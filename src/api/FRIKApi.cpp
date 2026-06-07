#define FRIK_API_EXPORTS
#include "FRIKAPI.h"

#include "FRIK.h"
#include "common/CommonUtils.h"
#include "f4vr/F4VRSkelly.h"
#include "f4vr/F4VRUtils.h"
#include "skeleton/HandPose.h"
#include "skeleton/HandPoseData.h"
#include "skeleton/Skeleton.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
    using namespace frik;
    using namespace frik::api;
    using namespace frik::skeleton::data;

    /**
     * Used to keep track of external tags blocking offhand gripping to prevent conflicts between client mods.
     * The actual tag values are not relevant to FRIK, only the fact that there is at least one tag blocking it.
     */
    std::unordered_set<std::string> g_offHandGripBlockingTags;
    constexpr std::string_view LEGACY_API_HAND_POSE_TAG = "frik.api.legacy";

    struct ExternalHandAuthorityEntry
    {
        std::string tag;
        RE::NiTransform worldTarget;
        int priority = 0;
        std::uint64_t generation = 0;
    };

    struct SelectedExternalHandAuthority
    {
        const ExternalHandAuthorityEntry* entry = nullptr;
    };

    std::array<std::vector<ExternalHandAuthorityEntry>, 2> g_externalHandAuthorities;
    std::uint64_t g_externalHandAuthorityGeneration = 0;

    std::size_t handAuthorityIndex(const bool isLeft) { return isLeft ? 1U : 0U; }

    SelectedExternalHandAuthority selectExternalHandAuthority(const std::vector<ExternalHandAuthorityEntry>& entries)
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
        return SelectedExternalHandAuthority{ .entry = best };
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

    std::optional<std::string> getNormalizedTag(const char* tag)
    {
        if (!f4cf::common::hasNonWhitespaceText(tag)) {
            return std::nullopt;
        }

        return f4cf::common::trim(tag);
    }

    HandFingersPose makeUniformFingerPose(const float thumb, const float index, const float middle, const float ring, const float pinky)
    {
        return HandFingersPose{
            FingerPose{ thumb, thumb, thumb },
            FingerPose{ index, index, index },
            FingerPose{ middle, middle, middle },
            FingerPose{ ring, ring, ring },
            FingerPose{ pinky, pinky, pinky }
        };
    }

    HandFingersPose makeHandPoseFromApiData(const FRIKApi::HandPoseData& handPose, const HandPoseKind kind = HandPoseKind::Custom)
    {
        return HandFingersPose{
            FingerPose{ handPose.thumb.prox, handPose.thumb.mid, handPose.thumb.dist, handPose.thumb.splay },
            FingerPose{ handPose.index.prox, handPose.index.mid, handPose.index.dist, handPose.index.splay },
            FingerPose{ handPose.middle.prox, handPose.middle.mid, handPose.middle.dist, handPose.middle.splay },
            FingerPose{ handPose.ring.prox, handPose.ring.mid, handPose.ring.dist, handPose.ring.splay },
            FingerPose{ handPose.pinky.prox, handPose.pinky.mid, handPose.pinky.dist, handPose.pinky.splay },
            handPose.palmPitch,
            handPose.palmYaw,
            kind
        };
    }

    HandFingersPose makeJointFingerPose(const float values[15])
    {
        return HandFingersPose{
            FingerPose{ values[0], values[1], values[2] },
            FingerPose{ values[3], values[4], values[5] },
            FingerPose{ values[6], values[7], values[8] },
            FingerPose{ values[9], values[10], values[11] },
            FingerPose{ values[12], values[13], values[14] }
        };
    }

    std::optional<HandFingersPose> makePredefinedHandPose(const FRIKApi::HandPoseKind handPose)
    {
        switch (handPose) {
        case FRIKApi::HandPoseKind::Open:
            return getOpenPose();
        case FRIKApi::HandPoseKind::Pointing:
            return getPointingPose();
        case FRIKApi::HandPoseKind::HoldingWeapon:
            return HandPose::getFixedPrimaryWeaponPose();
        case FRIKApi::HandPoseKind::OffhandGrip:
            return getOffhandWeaponGripPose();
        case FRIKApi::HandPoseKind::Attaboy:
            return getAttaboyPose();
        case FRIKApi::HandPoseKind::ThumbsUp:
            return getThumbsUpPose();
        case FRIKApi::HandPoseKind::Fist:
            return getFistPose();
        case FRIKApi::HandPoseKind::HoldingGun:
            return getGunGripPose();
        case FRIKApi::HandPoseKind::HoldingMelee:
            return getMeleeGripPose();
        default:
            return std::nullopt;
        }
    }

    FRIKApi::HandPoseTagState toApiHandPoseTagState(const HandPoseOverrideTagState state)
    {
        switch (state) {
        case HandPoseOverrideTagState::None:
            return FRIKApi::HandPoseTagState::None;
        case HandPoseOverrideTagState::Active:
            return FRIKApi::HandPoseTagState::Active;
        case HandPoseOverrideTagState::Overridden:
            return FRIKApi::HandPoseTagState::Overriden;
        default:
            return FRIKApi::HandPoseTagState::None;
        }
    }

    FRIKApi::HandPoseKind toApiHandPoseKind(const frik::skeleton::data::HandPoseKind kind)
    {
        switch (kind) {
        case HandPoseKind::Unset:
            return FRIKApi::HandPoseKind::Unset;
        case HandPoseKind::Custom:
            return FRIKApi::HandPoseKind::Custom;
        case HandPoseKind::Open:
            return FRIKApi::HandPoseKind::Open;
        case HandPoseKind::Pointing:
            return FRIKApi::HandPoseKind::Pointing;
        case HandPoseKind::HoldingWeapon:
            return FRIKApi::HandPoseKind::HoldingWeapon;
        case HandPoseKind::OffhandGrip:
            return FRIKApi::HandPoseKind::OffhandGrip;
        case HandPoseKind::Attaboy:
            return FRIKApi::HandPoseKind::Attaboy;
        case HandPoseKind::ThumbsUp:
            return FRIKApi::HandPoseKind::ThumbsUp;
        default:
            return FRIKApi::HandPoseKind::Unset;
        }
    }

    std::uint32_t FRIK_CALL getVersion()
    {
        return FRIK_API_VERSION;
    }

    const char* FRIK_CALL getModVersion()
    {
        // Safe to return pointer to static data
        static_assert(Version::NAME.back() != '\0' || true, "Version must be backed by a string literal");
        return Version::NAME.data();
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
        if (!f4cf::common::hasNonWhitespaceText(tag)) {
            return false;
        }

        const std::string normalizedTag = f4cf::common::trim(tag);
        if (block) {
            g_offHandGripBlockingTags.emplace(normalizedTag);
        } else {
            g_offHandGripBlockingTags.erase(normalizedTag);
        }

        logger::sample("API blockOffHandWeaponGripping tag:'{}' block:{} activeBlocks:{}", normalizedTag, block, g_offHandGripBlockingTags.size());
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
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag) {
            return FRIKApi::HandPoseTagState::None;
        }

        return toApiHandPoseTagState(HandPose::getHandPoseSetTagState(getIsLeftForHandEnum(hand), *normalizedTag));
    }

    FRIKApi::HandPoseKind FRIK_CALL getCurrentHandPose(const FRIKApi::Hand hand)
    {
        return toApiHandPoseKind(HandPose::getCurrentHandPoseKind(getIsLeftForHandEnum(hand)));
    }

    bool FRIK_CALL setHandPose(const char* tag, const FRIKApi::Hand hand, const FRIKApi::HandPoseKind handPose)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag) {
            return false;
        }

        const bool isLeft = getIsLeftForHandEnum(hand);
        if (handPose == FRIKApi::HandPoseKind::Unset) {
            HandPose::clearHandPoseOverride(isLeft, *normalizedTag);
            return true;
        }

        if (handPose == FRIKApi::HandPoseKind::Custom) {
            return false;
        }

        const auto pose = makePredefinedHandPose(handPose);
        if (!pose) {
            return false;
        }

        HandPose::setHandPoseOverride(isLeft, *normalizedTag, *pose, false);
        return true;
    }

    bool FRIK_CALL setHandPoseCustomFingerPositions(const char* tag, const FRIKApi::Hand hand, const float thumb, const float index, const float middle, const float ring,
        const float pinky)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag) {
            return false;
        }

        HandPose::setHandPoseOverride(getIsLeftForHandEnum(hand), *normalizedTag, makeUniformFingerPose(thumb, index, middle, ring, pinky), false);
        return true;
    }

    bool FRIK_CALL setHandPoseCustom(const char* tag, const FRIKApi::Hand hand, const FRIKApi::HandPoseData& handPose, const bool forceTop)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag) {
            return false;
        }

        HandPose::setHandPoseOverride(getIsLeftForHandEnum(hand), *normalizedTag, makeHandPoseFromApiData(handPose), forceTop);
        return true;
    }

    bool FRIK_CALL clearHandPose(const char* tag, const FRIKApi::Hand hand)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag) {
            return false;
        }

        HandPose::clearHandPoseOverride(getIsLeftForHandEnum(hand), *normalizedTag);
        return true;
    }

    void FRIK_CALL setHandPoseFingerPositions(const FRIKApi::Hand hand, const float thumb, const float index, const float middle, const float ring, const float pinky)
    {
        HandPose::setHandPoseOverride(getIsLeftForHandEnum(hand), LEGACY_API_HAND_POSE_TAG, makeUniformFingerPose(thumb, index, middle, ring, pinky), false);
    }

    void FRIK_CALL clearHandPoseFingerPositions(const FRIKApi::Hand hand)
    {
        HandPose::clearHandPoseOverride(getIsLeftForHandEnum(hand), LEGACY_API_HAND_POSE_TAG);
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

    bool FRIK_CALL setHandPoseWithPriority(const char* tag, const FRIKApi::Hand hand, const FRIKApi::HandPoseKind handPose, const int priority)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag || priority < 0) {
            return false;
        }

        const bool isLeft = getIsLeftForHandEnum(hand);
        if (handPose == FRIKApi::HandPoseKind::Unset) {
            HandPose::clearHandPoseOverride(isLeft, *normalizedTag);
            return true;
        }

        if (handPose == FRIKApi::HandPoseKind::Custom) {
            return false;
        }

        const auto pose = makePredefinedHandPose(handPose);
        if (!pose) {
            return false;
        }

        HandPose::setHandPoseOverrideWithPriority(isLeft, *normalizedTag, *pose, priority);
        return true;
    }

    RE::NiTransform FRIK_CALL getHandWorldTransform(const FRIKApi::Hand hand)
    {
        const auto* skelly = g_frik.getSkeleton();
        if (!skelly) {
            return RE::NiTransform();
        }
        return skelly->getHandWorldTransform(getIsLeftForHandEnum(hand));
    }

    bool FRIK_CALL setHandPoseCustomFingerPositionsWithPriority(const char* tag, const FRIKApi::Hand hand, const float thumb, const float index, const float middle,
        const float ring, const float pinky, const int priority)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag || priority < 0) {
            return false;
        }

        HandPose::setHandPoseOverrideWithPriority(getIsLeftForHandEnum(hand), *normalizedTag, makeUniformFingerPose(thumb, index, middle, ring, pinky), priority);
        return true;
    }

    bool FRIK_CALL setHandPoseCustomJointPositionsWithPriority(const char* tag, const FRIKApi::Hand hand, const float values[15], const int priority)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag || !values || priority < 0) {
            return false;
        }

        HandPose::setHandPoseOverrideWithPriority(getIsLeftForHandEnum(hand), *normalizedTag, makeJointFingerPose(values), priority);
        return true;
    }

    bool FRIK_CALL applyExternalHandWorldTransform(const char* tag, const FRIKApi::Hand hand, const RE::NiTransform& worldTarget, const int priority)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag || priority < 0 || !g_frik.getSkeleton()) {
            return false;
        }

        const bool isLeft = getIsLeftForHandEnum(hand);
        auto& entries = g_externalHandAuthorities[handAuthorityIndex(isLeft)];
        auto it = std::ranges::find_if(entries, [&](const ExternalHandAuthorityEntry& entry) { return entry.tag == *normalizedTag; });
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

        const auto selected = selectExternalHandAuthority(entries);
        if (!selected.entry) {
            return false;
        }

        if (selected.entry->tag != *normalizedTag) {
            return true;
        }

        auto* skelly = g_frik.getSkeleton();
        if (!skelly || !skelly->applyExternalHandWorldTransform(isLeft, selected.entry->worldTarget)) {
            return false;
        }

        g_frik.refreshAfterExternalHandAuthority(isLeft);
        return true;
    }

    bool FRIK_CALL clearExternalHandWorldTransform(const char* tag, const FRIKApi::Hand hand)
    {
        const auto normalizedTag = getNormalizedTag(tag);
        if (!normalizedTag) {
            return false;
        }

        const bool isLeft = getIsLeftForHandEnum(hand);
        auto& entries = g_externalHandAuthorities[handAuthorityIndex(isLeft)];
        const auto oldSize = entries.size();
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const ExternalHandAuthorityEntry& entry) { return entry.tag == *normalizedTag; }), entries.end());
        if (entries.size() == oldSize) {
            return true;
        }

        auto* skelly = g_frik.getSkeleton();
        if (!skelly) {
            return false;
        }

        const auto selected = selectExternalHandAuthority(entries);
        if (!selected.entry) {
            if (!skelly->restoreTrackedHandAfterExternalAuthority(isLeft)) {
                return false;
            }
            g_frik.refreshAfterExternalHandAuthority(isLeft);
            return true;
        }

        if (!skelly->applyExternalHandWorldTransform(isLeft, selected.entry->worldTarget)) {
            return false;
        }

        g_frik.refreshAfterExternalHandAuthority(isLeft);
        return true;
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
        .getHandWorldTransform = &getHandWorldTransform,
        .setHandPoseCustomFingerPositionsWithPriority = &setHandPoseCustomFingerPositionsWithPriority,
        .setHandPoseCustomJointPositionsWithPriority = &setHandPoseCustomJointPositionsWithPriority,
        .applyExternalHandWorldTransform = &applyExternalHandWorldTransform,
        .clearExternalHandWorldTransform = &clearExternalHandWorldTransform,
        .setHandPoseCustomLocalTransformsWithPriority = nullptr,
        .getHandPoseLocalTransformsForJointPositions = nullptr
    };
}

namespace frik::api
{
    void clearExternalHandAuthorityStateForSkeletonRelease()
    {
        for (auto& entries : g_externalHandAuthorities) {
            entries.clear();
        }
        g_externalHandAuthorityGeneration = 0;
    }

    FRIK_API const FRIKApi* FRIK_CALL FRIKAPI_GetApi()
    {
        return &FRIK_API_FUNCTIONS_TABLE;
    }
}
