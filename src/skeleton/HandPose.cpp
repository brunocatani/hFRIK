#include "HandPose.h"
#include "HandPoseLocalTransformPolicy.h"
#include "HandPoseStackPolicy.h"
#include "HandPoseWeaponPolicy.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include "Config.h"
#include "FRIK.h"
#include "common/Quaternion.h"

using namespace common;

// TODO: this code is terrible, primary it doesn't handle multiple code paths set hand pose, release will release all of them
namespace frik
{
    constexpr int FINGERS_COUNT = 15;

    static const std::string LEFT_HAND_FINGERS[] = {
        "LArm_Finger11", "LArm_Finger12", "LArm_Finger13", "LArm_Finger21", "LArm_Finger22", "LArm_Finger23", "LArm_Finger31", "LArm_Finger32", "LArm_Finger33", "LArm_Finger41",
        "LArm_Finger42", "LArm_Finger43", "LArm_Finger51", "LArm_Finger52", "LArm_Finger53"
    };
    static const std::string RIGHT_HAND_FINGERS[] = {
        "RArm_Finger11", "RArm_Finger12", "RArm_Finger13", "RArm_Finger21", "RArm_Finger22", "RArm_Finger23", "RArm_Finger31", "RArm_Finger32", "RArm_Finger33", "RArm_Finger41",
        "RArm_Finger42", "RArm_Finger43", "RArm_Finger51", "RArm_Finger52", "RArm_Finger53"
    };

    std::map<std::string, RE::NiTransform, CaseInsensitiveComparator> handClosed;
    std::map<std::string, RE::NiTransform, CaseInsensitiveComparator> handOpen;

    std::map<std::string, int> boneToIndexMap;

    std::map<std::string, float> handPapyrusPose;
    std::map<std::string, bool> handPapyrusHasControl;
    std::map<std::string, RE::NiTransform, CaseInsensitiveComparator> handLocalTransformOverride;
    std::map<std::string, bool> handLocalTransformHasControl;
    std::map<std::string, float, CaseInsensitiveComparator> handPoseSplay;
    std::map<std::string, bool, CaseInsensitiveComparator> handPoseSplayHasControl;
    std::array<float, 2> handPosePalmPitch = { 0.0f, 0.0f };
    std::array<float, 2> handPosePalmYaw = { 0.0f, 0.0f };
    std::array<bool, 2> handPosePalmHasControl = { false, false };

    static constexpr float HAND_FINGERS_HOLDING_GUN_POSE[] = { 0.7f, 0.4f, 0.5f, 0.9f, 0.6f, 0.5f, 0.3f, 0.5f, 0.5f, 0.1f, 0.5f, 0.5f, 0.0f, 0.5f, 0.7f };
    static constexpr float HAND_FINGERS_HOLDING_MELEE_POSE[] = { 0.7f, 0.5f, 0.8f, 0.4f, 0.3f, 0.9f, 0.1f, 0.5f, 0.9f, 0.0f, 0.5f, 0.9f, 0.0f, 0.4f, 0.9f };
    static constexpr float HAND_FINGERS_POINTING_POSE[] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    static constexpr float HAND_FINGERS_ATTABOY_HOLDING_POSE[] = { 0.7f, 1.2f, 1.3f, 1.1f, 1.1f, 1.2f, 0.8f, 0.6f, 1.0f, 0.4f, 0.8f, 1.0f, 0.1f, 1.0f, 1.4f };

    static bool _handPoseSet[2] = { false, false };
    static constexpr float OFFHAND_FINGERS_GRIP_POSE[] = { 1.0f, 1.0f, 0.9f, 0.6f, 0.6f, 0.6f, 0.5f, 0.6f, 0.55f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
    static constexpr float HAND_FINGERS_FIST_POSE[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    static constexpr float HAND_FINGERS_OPEN_POSE[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    static constexpr float HAND_FINGERS_THUMBS_UP_POSE[] = { 1.6f, 1.4f, 1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    static std::size_t handPosePalmIndex(const bool isLeft)
    {
        return isLeft ? 1U : 0U;
    }

    static api::FRIKApi::FingerPoseData makeFingerPoseData(const float prox, const float mid, const float dist, const float splay = 0.0f)
    {
        return api::FRIKApi::FingerPoseData{
            .prox = prox,
            .mid = mid,
            .dist = dist,
            .splay = splay,
        };
    }

    static api::FRIKApi::HandPoseData makeHandPoseDataFromPerBoneValues(const float* values)
    {
        return api::FRIKApi::HandPoseData{
            .thumb = makeFingerPoseData(values[0], values[1], values[2]),
            .index = makeFingerPoseData(values[3], values[4], values[5]),
            .middle = makeFingerPoseData(values[6], values[7], values[8]),
            .ring = makeFingerPoseData(values[9], values[10], values[11]),
            .pinky = makeFingerPoseData(values[12], values[13], values[14]),
        };
    }

    static api::FRIKApi::HandPoseData makeHandPoseDataFromFiveFingerValues(
        const float thumb,
        const float index,
        const float middle,
        const float ring,
        const float pinky)
    {
        return api::FRIKApi::HandPoseData{
            .thumb = makeFingerPoseData(thumb, thumb, thumb),
            .index = makeFingerPoseData(index, index, index),
            .middle = makeFingerPoseData(middle, middle, middle),
            .ring = makeFingerPoseData(ring, ring, ring),
            .pinky = makeFingerPoseData(pinky, pinky, pinky),
        };
    }

    static float fingerSplayAt(const api::FRIKApi::HandPoseData& poseData, const int fingerIndex)
    {
        switch (fingerIndex) {
        case 0:
            return poseData.thumb.splay;
        case 1:
            return poseData.index.splay;
        case 2:
            return poseData.middle.splay;
        case 3:
            return poseData.ring.splay;
        default:
            return poseData.pinky.splay;
        }
    }

    static void copyPoseDataToEntry(const api::FRIKApi::HandPoseData& poseData, HandPoseEntry& entry)
    {
        entry.poseData = poseData;
        entry.fingers[0] = poseData.thumb.prox;
        entry.fingers[1] = poseData.index.prox;
        entry.fingers[2] = poseData.middle.prox;
        entry.fingers[3] = poseData.ring.prox;
        entry.fingers[4] = poseData.pinky.prox;
        entry.perBoneFingers[0] = poseData.thumb.prox;
        entry.perBoneFingers[1] = poseData.thumb.mid;
        entry.perBoneFingers[2] = poseData.thumb.dist;
        entry.perBoneFingers[3] = poseData.index.prox;
        entry.perBoneFingers[4] = poseData.index.mid;
        entry.perBoneFingers[5] = poseData.index.dist;
        entry.perBoneFingers[6] = poseData.middle.prox;
        entry.perBoneFingers[7] = poseData.middle.mid;
        entry.perBoneFingers[8] = poseData.middle.dist;
        entry.perBoneFingers[9] = poseData.ring.prox;
        entry.perBoneFingers[10] = poseData.ring.mid;
        entry.perBoneFingers[11] = poseData.ring.dist;
        entry.perBoneFingers[12] = poseData.pinky.prox;
        entry.perBoneFingers[13] = poseData.pinky.mid;
        entry.perBoneFingers[14] = poseData.pinky.dist;
    }

    static void sortHandPoseStack(std::vector<HandPoseEntry>& stack)
    {
        std::sort(stack.begin(), stack.end(), [](const HandPoseEntry& a, const HandPoseEntry& b) {
            return hand_pose_stack_policy::sortsBefore(a.priority, a.sequence, b.priority, b.sequence);
        });
    }

    /**
     * Get the pose value for the given bone either for melee or gun holding hand pose.
     */
    float getHandBonePose(const std::string& bone, const bool melee)
    {
        const auto handPose = melee ? HAND_FINGERS_HOLDING_MELEE_POSE : HAND_FINGERS_HOLDING_GUN_POSE;
        return handPose[boneToIndexMap[bone]];
    }

    static void copyDataIntoHand(const std::vector<float>& fingerData, std::map<std::string, RE::NiTransform, CaseInsensitiveComparator>& hand, const char* finger)
    {
        for (int row = 0; row < 3; row++) {
            for (int col = 0; col < 4; col++) {
                hand[finger].rotate.entry[row][col] = fingerData[row * 4 + col];
            }
        }
    }

    static void copyDataIntoHand(std::vector<std::vector<float>> data, std::map<std::string, RE::NiTransform, CaseInsensitiveComparator>& hand)
    {
        // Left hand fingers
        copyDataIntoHand(data[0], hand, "LArm_Finger11");
        copyDataIntoHand(data[1], hand, "LArm_Finger12");
        copyDataIntoHand(data[2], hand, "LArm_Finger13");
        copyDataIntoHand(data[3], hand, "LArm_Finger21");
        copyDataIntoHand(data[4], hand, "LArm_Finger22");
        copyDataIntoHand(data[5], hand, "LArm_Finger23");
        copyDataIntoHand(data[6], hand, "LArm_Finger31");
        copyDataIntoHand(data[7], hand, "LArm_Finger32");
        copyDataIntoHand(data[8], hand, "LArm_Finger33");
        copyDataIntoHand(data[9], hand, "LArm_Finger41");
        copyDataIntoHand(data[10], hand, "LArm_Finger42");
        copyDataIntoHand(data[11], hand, "LArm_Finger43");
        copyDataIntoHand(data[12], hand, "LArm_Finger51");
        copyDataIntoHand(data[13], hand, "LArm_Finger52");
        copyDataIntoHand(data[14], hand, "LArm_Finger53");

        // Right hand fingers
        copyDataIntoHand(data[15], hand, "RArm_Finger11");
        copyDataIntoHand(data[16], hand, "RArm_Finger12");
        copyDataIntoHand(data[17], hand, "RArm_Finger13");
        copyDataIntoHand(data[18], hand, "RArm_Finger21");
        copyDataIntoHand(data[19], hand, "RArm_Finger22");
        copyDataIntoHand(data[20], hand, "RArm_Finger23");
        copyDataIntoHand(data[21], hand, "RArm_Finger31");
        copyDataIntoHand(data[22], hand, "RArm_Finger32");
        copyDataIntoHand(data[23], hand, "RArm_Finger33");
        copyDataIntoHand(data[24], hand, "RArm_Finger41");
        copyDataIntoHand(data[25], hand, "RArm_Finger42");
        copyDataIntoHand(data[26], hand, "RArm_Finger43");
        copyDataIntoHand(data[27], hand, "RArm_Finger51");
        copyDataIntoHand(data[28], hand, "RArm_Finger52");
        copyDataIntoHand(data[29], hand, "RArm_Finger53");
    }

    void initHandPoses(const bool inPowerArmor)
    {
        if (boneToIndexMap.empty()) {
            for (auto i = 0; i < FINGERS_COUNT; i++) {
                boneToIndexMap[LEFT_HAND_FINGERS[i]] = i;
                boneToIndexMap[RIGHT_HAND_FINGERS[i]] = i;
            }
        }

        std::vector<std::vector<float>> data;

        // pulled from the game engine while running idle animations

        //closed fist first
        data.push_back({ 0.849409F, -0.270577F, 0.453092F, 0, -0.382631F, 0.275533F, 0.881859F, 0, -0.363453F, -0.922426F, 0.130509F, 0 });
        data.push_back({ 0.698533F, -0.713903F, 0.048938F, 0, 0.710545F, 0.700093F, 0.070685F, 0, -0.084723F, -0.014603F, 0.996297F, 0 });
        data.push_back({ 0.125157F, -0.992116F, -0.006447F, 0, 0.990953F, 0.125323F, -0.048036F, 0, 0.048466F, -0.000376F, 0.998825F, 0 });
        data.push_back({ 0.088989F, -0.995196F, 0.04083F, 0, 0.995157F, 0.090554F, 0.038248F, 0, -0.041762F, 0.037228F, 0.998434F, 0 });
        data.push_back({ -0.473616F, -0.880732F, 0, 0, 0.880732F, -0.473616F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ -0.123119F, -0.992392F, 0, 0, 0.992392F, -0.123119F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.159314F, -0.982871F, 0.09265F, 0, 0.983889F, 0.150362F, -0.096712F, 0, 0.081124F, 0.106565F, 0.990991F, 0 });
        data.push_back({ -0.45663F, -0.889657F, 0, 0, 0.889657F, -0.45663F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ -0.076698F, -0.997054F, 0, 0, 0.997054F, -0.076698F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.123006F, -0.978335F, 0.166524F, 0, 0.978335F, 0.091386F, -0.185766F, 0, 0.166524F, 0.185766F, 0.96838F, 0 });
        data.push_back({ -0.366717F, -0.930333F, 0, 0, 0.930333F, -0.366717F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.324171F, -0.945999F, 0, 0, 0.945999F, 0.324171F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.204525F, -0.935955F, 0.286631F, 0, 0.952761F, 0.123178F, -0.277623F, 0, 0.224536F, 0.329871F, 0.916934F, 0 });
        data.push_back({ -0.190355F, -0.981715F, -0.00044F, 0, 0.981715F, -0.190355F, -0.000533F, 0, 0.00044F, -0.000533F, 1, 0 });
        data.push_back({ -0.188246F, -0.982122F, 0, 0, 0.982122F, -0.188246F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.752071F, -0.282712F, -0.595368F, 0, -0.397682F, 0.525706F, -0.751986F, 0, 0.525584F, 0.802314F, 0.282939F, 0 });
        data.push_back({ 0.556184F, -0.830294F, -0.035639F, 0, 0.826703F, 0.557145F, -0.078435F, 0, 0.084981F, 0.014162F, 0.996282F, 0 });
        data.push_back({ 0.620726F, -0.783447F, 0.030166F, 0, 0.782545F, 0.621458F, 0.037589F, 0, -0.048196F, 0.000274F, 0.998838F, 0 });
        data.push_back({ 0.38695F, -0.915355F, -0.111332F, 0, 0.917694F, 0.394073F, -0.050434F, 0, 0.090038F, -0.082654F, 0.992503F, 0 });
        data.push_back({ -0.152033F, -0.988376F, 0, 0, 0.988376F, -0.152033F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.397566F, -0.917574F, 0, 0, 0.917574F, 0.397566F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.076671F, -0.99201F, -0.100188F, 0, 0.996805F, 0.078521F, -0.014653F, 0, 0.022403F, -0.098745F, 0.994861F, 0 });
        data.push_back({ -0.068391F, -0.997659F, 0, 0, 0.997659F, -0.068391F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ -0.050058F, -0.998746F, 0, 0, 0.998746F, -0.050058F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.068248F, -0.982702F, -0.172158F, 0, 0.997656F, 0.068079F, 0.006893F, 0, 0.004947F, -0.172225F, 0.985045F, 0 });
        data.push_back({ 0.093539F, -0.995616F, 0, 0, 0.995616F, 0.093539F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ -0.33522F, -0.94214F, 0, 0, 0.94214F, -0.33522F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.257096F, -0.93156F, -0.257096F, 0, 0.955995F, 0.206258F, 0.208641F, 0, -0.141334F, -0.299423F, 0.943595F, 0 });
        data.push_back({ -0.21201F, -0.977267F, -0.000434F, 0, 0.977267F, -0.21201F, -0.000538F, 0, 0.000434F, -0.000538F, 1, 0 });
        data.push_back({ -0.276492F, -0.961017F, 0, 0, 0.961017F, -0.276492F, 0, 0, 0, 0, 1, 0 });

        copyDataIntoHand(data, handClosed);

        data.erase(data.begin(), data.end());

        data.push_back({ 0.617716F, -0.400404F, 0.676834F, 0, -0.65398F, 0.216427F, 0.724893F, 0, -0.436735F, -0.890414F, -0.128165F, 0 });
        data.push_back({ 0.899514F, -0.434294F, -0.048362F, 0, 0.435479F, 0.89999F, 0.019389F, 0, 0.035107F, -0.038501F, 0.998642F, 0 });
        data.push_back({ 0.945701F, -0.321798F, -0.045777F, 0, 0.321435F, 0.946808F, -0.015267F, 0, 0.048255F, -0.000276F, 0.998835F, 0 });
        data.push_back({ 0.990258F, -0.114774F, 0.078839F, 0, 0.111225F, 0.992634F, 0.048027F, 0, -0.08377F, -0.03879F, 0.99573F, 0 });
        data.push_back({ 0.958294F, -0.285783F, 0, 0, 0.285783F, 0.958294F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.992354F, -0.123425F, 0, 0, 0.123425F, 0.992354F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.951661F, -0.27608F, -0.134618F, 0, 0.266956F, 0.960211F, -0.082032F, 0, 0.151909F, 0.04213F, 0.987496F, 0 });
        data.push_back({ 0.902528F, -0.430632F, -0.000153F, 0, 0.430632F, 0.902527F, 0.000674F, 0, -0.000153F, -0.000674F, 1, 0 });
        data.push_back({ 0.953147F, -0.302508F, 0.000106F, 0, 0.302508F, 0.953147F, -0.000683F, 0, 0.000106F, 0.000683F, 1, 0 });
        data.push_back({ 0.919043F, -0.392269F, -0.038525F, 0, 0.384414F, 0.913631F, -0.132302F, 0, 0.087095F, 0.106782F, 0.990461F, 0 });
        data.push_back({ 0.927023F, -0.375003F, 0, 0, 0.375003F, 0.927023F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.984968F, -0.172734F, 0, 0, 0.172734F, 0.984968F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.825976F, -0.557004F, -0.086665F, 0, 0.534941F, 0.822993F, -0.191102F, 0, 0.17777F, 0.111485F, 0.977737F, 0 });
        data.push_back({ 0.935958F, -0.352111F, 0, 0, 0.352111F, 0.935958F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.833619F, -0.552339F, 0, 0, 0.552339F, 0.833619F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.584889F, -0.400611F, -0.705277F, 0, -0.656401F, 0.277021F, -0.70171F, 0, 0.47649F, 0.873367F, -0.100935F, 0 });
        data.push_back({ 0.812239F, -0.583324F, 0, 0, 0.583324F, 0.812239F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.970436F, -0.241361F, 0, 0, 0.241361F, 0.970436F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.969328F, -0.20464F, -0.136108F, 0, 0.195507F, 0.977633F, -0.077531F, 0, 0.148929F, 0.048543F, 0.987656F, 0 });
        data.push_back({ 0.949484F, -0.313814F, 0, 0, 0.313814F, 0.949484F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.980211F, -0.197957F, 0, 0, 0.197957F, 0.980211F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.954206F, -0.298892F, 0.01245F, 0, 0.29779F, 0.953005F, 0.055697F, 0, -0.028512F, -0.049439F, 0.99837F, 0 });
        data.push_back({ 0.903441F, -0.428712F, 0, 0, 0.428712F, 0.903441F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.967689F, -0.252149F, 0, 0, 0.252149F, 0.967689F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.926338F, -0.376682F, -0.002837F, 0, 0.37216F, 0.914003F, 0.161543F, 0, -0.058257F, -0.1507F, 0.986862F, 0 });
        data.push_back({ 0.914348F, -0.40493F, 0, 0, 0.40493F, 0.914348F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.919149F, -0.39391F, 0, 0, 0.39391F, 0.919149F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.921646F, -0.376617F, 0.09343F, 0, 0.345979F, 0.906603F, 0.241599F, 0, -0.175694F, -0.190344F, 0.965868F, 0 });
        data.push_back({ 0.957083F, -0.289814F, 0, 0, 0.289814F, 0.957083F, 0, 0, 0, 0, 1, 0 });
        data.push_back({ 0.758452F, -0.651728F, 0, 0, 0.651728F, 0.758452F, 0, 0, 0, 0, 1, 0 });

        copyDataIntoHand(data, handOpen);

        if (inPowerArmor) {
            handOpen["LArm_Finger11"].translate = RE::NiPoint3(3.993323F, -4.156268F, 3.585619F);
            handOpen["LArm_Finger12"].translate = RE::NiPoint3(2.893830F, 0.000042F, 0.000004F);
            handOpen["LArm_Finger13"].translate = RE::NiPoint3(4.687409F, 0, 0);
            handOpen["LArm_Finger21"].translate = RE::NiPoint3(8.474635F, -2.161191F, 3.789806F);
            handOpen["LArm_Finger22"].translate = RE::NiPoint3(2.613208F, 0.000026F, 0.000011F);
            handOpen["LArm_Finger23"].translate = RE::NiPoint3(5.145684F, 0, 0);
            handOpen["LArm_Finger31"].translate = RE::NiPoint3(8.151892F, -2.576661F, 1.100114F);
            handOpen["LArm_Finger32"].translate = RE::NiPoint3(3.722714F, 0.000021F, -0.000004F);
            handOpen["LArm_Finger33"].translate = RE::NiPoint3(4.984375F, 0, 0);
            handOpen["LArm_Finger41"].translate = RE::NiPoint3(7.967844F, -2.258833F, -1.337387F);
            handOpen["LArm_Finger42"].translate = RE::NiPoint3(2.933939F, 0.000027F, 0.000004F);
            handOpen["LArm_Finger43"].translate = RE::NiPoint3(5.102559F, 0, 0);
            handOpen["LArm_Finger51"].translate = RE::NiPoint3(8.365221F, -2.603350F, -3.706458F);
            handOpen["LArm_Finger52"].translate = RE::NiPoint3(2.128304F, 0.000018F, 0.000003F);
            handOpen["LArm_Finger53"].translate = RE::NiPoint3(4.594295F, 0, 0);
            handOpen["RArm_Finger11"].translate = RE::NiPoint3(3.993090F, -4.156340F, -3.585553F);
            handOpen["RArm_Finger12"].translate = RE::NiPoint3(2.893783F, 0.000042F, 0.000004F);
            handOpen["RArm_Finger13"].translate = RE::NiPoint3(4.686954F, 0, 0);
            handOpen["RArm_Finger21"].translate = RE::NiPoint3(8.474229F, -2.161169F, -3.789712F);
            handOpen["RArm_Finger22"].translate = RE::NiPoint3(2.613165F, 0.000026F, 0.000011F);
            handOpen["RArm_Finger23"].translate = RE::NiPoint3(5.145271F, 0, 0);
            handOpen["RArm_Finger31"].translate = RE::NiPoint3(8.151529F, -2.576689F, -1.100008F);
            handOpen["RArm_Finger32"].translate = RE::NiPoint3(3.722677F, 0.000021F, -0.000004F);
            handOpen["RArm_Finger33"].translate = RE::NiPoint3(4.973974F, 0, 0);
            handOpen["RArm_Finger41"].translate = RE::NiPoint3(7.967505F, -2.258873F, 1.337498F);
            handOpen["RArm_Finger42"].translate = RE::NiPoint3(2.933841F, 0.000027F, 0.000004F);
            handOpen["RArm_Finger43"].translate = RE::NiPoint3(5.102017F, 0, 0);
            handOpen["RArm_Finger51"].translate = RE::NiPoint3(8.364894F, -2.603419F, 3.706582F);
            handOpen["RArm_Finger52"].translate = RE::NiPoint3(2.128275F, 0.000018F, 0.000003F);
            handOpen["RArm_Finger53"].translate = RE::NiPoint3(4.593989F, 0, 0);
        } else {
            handOpen["LArm_Finger11"].translate = RE::NiPoint3(1.582972F, -1.262648F, 1.853201F);
            handOpen["LArm_Finger12"].translate = RE::NiPoint3(3.569515F, 0.000042F, 0.000004F);
            handOpen["LArm_Finger13"].translate = RE::NiPoint3(2.401824F, 0, 0);
            handOpen["LArm_Finger21"].translate = RE::NiPoint3(7.501364F, 0.430291F, 2.277657F);
            handOpen["LArm_Finger22"].translate = RE::NiPoint3(3.018186F, 0.000026F, 0.000011F);
            handOpen["LArm_Finger23"].translate = RE::NiPoint3(1.850236F, 0, 0);
            handOpen["LArm_Finger31"].translate = RE::NiPoint3(7.595781F, 0.62098F, 0.457392F);
            handOpen["LArm_Finger32"].translate = RE::NiPoint3(3.091653F, 0.000021F, -0.000004F);
            handOpen["LArm_Finger33"].translate = RE::NiPoint3(2.187974F, 0, 0);
            handOpen["LArm_Finger41"].translate = RE::NiPoint3(7.464033F, 0.350152F, -1.438817F);
            handOpen["LArm_Finger42"].translate = RE::NiPoint3(2.664419F, 0.000027F, 0.000004F);
            handOpen["LArm_Finger43"].translate = RE::NiPoint3(1.89974F, 0, 0);
            handOpen["LArm_Finger51"].translate = RE::NiPoint3(6.637259F, -0.35742F, -3.01848F);
            handOpen["LArm_Finger52"].translate = RE::NiPoint3(2.238261F, 0.000018F, 0.000003F);
            handOpen["LArm_Finger53"].translate = RE::NiPoint3(1.665912F, 0, 0);
            handOpen["RArm_Finger11"].translate = RE::NiPoint3(1.582972F, -1.262648F, -1.853201F);
            handOpen["RArm_Finger12"].translate = RE::NiPoint3(3.569515F, 0.000042F, 0.000004F);
            handOpen["RArm_Finger13"].translate = RE::NiPoint3(2.401824F, 0, 0);
            handOpen["RArm_Finger21"].translate = RE::NiPoint3(7.501364F, 0.430291F, -2.277657F);
            handOpen["RArm_Finger22"].translate = RE::NiPoint3(3.018186F, 0.000026F, 0.000011F);
            handOpen["RArm_Finger23"].translate = RE::NiPoint3(1.850236F, 0, 0);
            handOpen["RArm_Finger31"].translate = RE::NiPoint3(7.595781F, 0.62098F, -0.457392F);
            handOpen["RArm_Finger32"].translate = RE::NiPoint3(3.091653F, 0.000021F, -0.000004F);
            handOpen["RArm_Finger33"].translate = RE::NiPoint3(2.187974F, 0, 0);
            handOpen["RArm_Finger41"].translate = RE::NiPoint3(7.464033F, 0.350152F, 1.438817F);
            handOpen["RArm_Finger42"].translate = RE::NiPoint3(2.664419F, 0.000027F, 0.000004F);
            handOpen["RArm_Finger43"].translate = RE::NiPoint3(1.89974F, 0, 0);
            handOpen["RArm_Finger51"].translate = RE::NiPoint3(6.637259F, -0.35742F, 3.01848F);
            handOpen["RArm_Finger52"].translate = RE::NiPoint3(2.238261F, 0.000018F, 0.000003F);
            handOpen["RArm_Finger53"].translate = RE::NiPoint3(1.665912F, 0, 0);
        }
    }

    void setFingerPositionScalar(const bool isLeft, const float thumb, const float index, const float middle, const float ring, const float pinky)
    {
        const auto* const fingersArray = isLeft ? LEFT_HAND_FINGERS : RIGHT_HAND_FINGERS;
        for (auto i = 0; i < FINGERS_COUNT; i++) {
            handPapyrusHasControl[fingersArray[i]] = true;
        }

        if (isLeft) {
            handPapyrusPose["LArm_Finger11"] = thumb;
            handPapyrusPose["LArm_Finger12"] = thumb;
            handPapyrusPose["LArm_Finger13"] = thumb;
            handPapyrusPose["LArm_Finger21"] = index;
            handPapyrusPose["LArm_Finger22"] = index;
            handPapyrusPose["LArm_Finger23"] = index;
            handPapyrusPose["LArm_Finger31"] = middle;
            handPapyrusPose["LArm_Finger32"] = middle;
            handPapyrusPose["LArm_Finger33"] = middle;
            handPapyrusPose["LArm_Finger41"] = ring;
            handPapyrusPose["LArm_Finger42"] = ring;
            handPapyrusPose["LArm_Finger43"] = ring;
            handPapyrusPose["LArm_Finger51"] = pinky;
            handPapyrusPose["LArm_Finger52"] = pinky;
            handPapyrusPose["LArm_Finger53"] = pinky;
        } else {
            handPapyrusPose["RArm_Finger11"] = thumb;
            handPapyrusPose["RArm_Finger12"] = thumb;
            handPapyrusPose["RArm_Finger13"] = thumb;
            handPapyrusPose["RArm_Finger21"] = index;
            handPapyrusPose["RArm_Finger22"] = index;
            handPapyrusPose["RArm_Finger23"] = index;
            handPapyrusPose["RArm_Finger31"] = middle;
            handPapyrusPose["RArm_Finger32"] = middle;
            handPapyrusPose["RArm_Finger33"] = middle;
            handPapyrusPose["RArm_Finger41"] = ring;
            handPapyrusPose["RArm_Finger42"] = ring;
            handPapyrusPose["RArm_Finger43"] = ring;
            handPapyrusPose["RArm_Finger51"] = pinky;
            handPapyrusPose["RArm_Finger52"] = pinky;
            handPapyrusPose["RArm_Finger53"] = pinky;
        }
    }

    void setFingerJointPositions(const bool isLeft, const float values[15])
    {
        const auto* const fingersArray = isLeft ? LEFT_HAND_FINGERS : RIGHT_HAND_FINGERS;
        for (auto i = 0; i < FINGERS_COUNT; i++) {
            handPapyrusHasControl[fingersArray[i]] = true;
            handPapyrusPose[fingersArray[i]] = values[i];
        }
    }

    void restoreFingerPoseControl(const bool isLeft)
    {
        logger::debug("Hand pose: Restore control for {} hand", isLeft ? "Left" : "Right");
        const auto* const fingersArray = isLeft ? LEFT_HAND_FINGERS : RIGHT_HAND_FINGERS;
        for (auto i = 0; i < FINGERS_COUNT; i++) {
            handPapyrusHasControl[fingersArray[i]] = false;
        }
    }

    namespace
    {
        bool isFiniteRotation(const RE::NiMatrix3& rotation)
        {
            for (int row = 0; row < 3; ++row) {
                for (int column = 0; column < 3; ++column) {
                    if (!std::isfinite(rotation.entry[row][column])) {
                        return false;
                    }
                }
            }
            return true;
        }

        bool isFiniteLocalTransform(const RE::NiTransform& transform)
        {
            return isFiniteRotation(transform.rotate) &&
                   std::isfinite(transform.translate.x) &&
                   std::isfinite(transform.translate.y) &&
                   std::isfinite(transform.translate.z) &&
                   std::isfinite(transform.scale) &&
                   std::abs(transform.scale) > 0.0001f;
        }
    }

    bool buildFingerLocalTransformsForJointPositions(
        bool isLeft,
        const float values[15],
        api::FRIKApi::FingerLocalTransformOverride& outTransforms)
    {
        outTransforms = {};
        if (!hand_pose_local_transform_policy::shouldBuildBaselineLocalTransforms(values)) {
            return false;
        }

        for (std::size_t i = 0; i < hand_pose_local_transform_policy::kFingerLocalTransformCount; ++i) {
            const char* boneName = hand_pose_local_transform_policy::fingerLocalTransformBoneName(isLeft, i);
            if (!boneName) {
                outTransforms = {};
                return false;
            }

            const auto openIt = handOpen.find(boneName);
            const auto closedIt = handClosed.find(boneName);
            if (openIt == handOpen.end() || closedIt == handClosed.end()) {
                outTransforms = {};
                return false;
            }

            Quaternion openRotation;
            Quaternion closedRotation;
            openRotation.fromMatrix(openIt->second.rotate);
            closedRotation.fromMatrix(closedIt->second.rotate);
            closedRotation.slerp(hand_pose_local_transform_policy::sanitizePoseBlendValue(values[i]), openRotation);

            RE::NiTransform localTransform = openIt->second;
            localTransform.rotate = closedRotation.getMatrix();
            if (!std::isfinite(localTransform.scale) || std::abs(localTransform.scale) <= 0.0001f) {
                localTransform.scale = 1.0f;
            }

            if (!isFiniteLocalTransform(localTransform)) {
                outTransforms = {};
                return false;
            }

            outTransforms.localTransforms[i] = localTransform;
            outTransforms.enabledMask = static_cast<std::uint16_t>(outTransforms.enabledMask | (1U << i));
        }

        outTransforms.enabledMask &= hand_pose_local_transform_policy::kFullFingerLocalTransformMask;
        return outTransforms.enabledMask == hand_pose_local_transform_policy::kFullFingerLocalTransformMask;
    }

    void setPipboyHandPose()
    {
        const bool isLeft = !g_config.leftHandedPipBoy;  // pipboy hand gets pointing pose
        g_handPoseManager.setTaggedPosePerBone("FRIK_Pipboy", isLeft, HandPosePriority::Pipboy, HAND_FINGERS_POINTING_POSE);
    }

    void disablePipboyHandPose()
    {
        const bool isLeft = !g_config.leftHandedPipBoy;
        g_handPoseManager.clearTaggedPose("FRIK_Pipboy", isLeft);
    }

    void setConfigModeHandPose()
    {
        const bool isLeft = false ^ f4vr::isLeftHandedMode();
        g_handPoseManager.setTaggedPosePerBone("FRIK_ConfigMode", isLeft, HandPosePriority::ConfigMode, HAND_FINGERS_POINTING_POSE);
    }

    void disableConfigModePose()
    {
        const bool isLeft = false ^ f4vr::isLeftHandedMode();
        g_handPoseManager.clearTaggedPose("FRIK_ConfigMode", isLeft);
    }

    /**
     * Set/Release hand to/from pointing pose for primary hand.
     * Right hand is primary hand if left-handed mode is off, left hand otherwise.
     */
    void setForceHandPointingPose(const bool primaryHand, const bool forcePointing)
    {
        const bool isLeft = primaryHand ^ f4vr::isLeftHandedMode();
        if (forcePointing) {
            g_handPoseManager.setTaggedPosePerBone("FRIK_ForcePointing", isLeft, HandPosePriority::ConfigMode, HAND_FINGERS_POINTING_POSE);
        } else {
            g_handPoseManager.clearTaggedPose("FRIK_ForcePointing", isLeft);
        }
    }

    void setOffhandGripHandPose(const bool toSet)
    {
        const bool isLeft = f4vr::isLeftHandedMode();
        if (toSet) {
            g_handPoseManager.setTaggedPosePerBone("FRIK_OffhandGrip", isLeft, HandPosePriority::OffhandGrip, OFFHAND_FINGERS_GRIP_POSE);
        } else {
            g_handPoseManager.clearTaggedPose("FRIK_OffhandGrip", isLeft);
        }
    }

    void setAttaboyHandPose(const bool toSet)
    {
        if (toSet) {
            g_handPoseManager.setTaggedPosePerBone("FRIK_Attaboy", false, HandPosePriority::Attaboy, HAND_FINGERS_ATTABOY_HOLDING_POSE);
        } else {
            g_handPoseManager.clearTaggedPose("FRIK_Attaboy", false);
        }
    }

    /**
     * Set/Release hand to/from specific pose for explicitly right or left hand.
     * Legacy function — still used by internal callers. Routes through the tag system with a generic tag.
     */
    void setHandPoseOverride(const bool override, const bool rightHand, const float* handPose)
    {
        const bool isLeft = !rightHand;
        if (override) {
            g_handPoseManager.setTaggedPosePerBone("FRIK_Legacy", isLeft, HandPosePriority::External, handPose);
        } else {
            g_handPoseManager.clearTaggedPose("FRIK_Legacy", isLeft);
        }
    }

    // ========================================================================
    // HandPoseManager implementation
    // ========================================================================

    const api::FRIKApi::HandPoseData* HandPoseManager::getPredefinedPoseData(api::FRIKApi::HandPoseKind pose)
    {
        static const api::FRIKApi::HandPoseData fist = makeHandPoseDataFromPerBoneValues(HAND_FINGERS_FIST_POSE);
        static const api::FRIKApi::HandPoseData open = makeHandPoseDataFromPerBoneValues(HAND_FINGERS_OPEN_POSE);
        static const api::FRIKApi::HandPoseData pointing = makeHandPoseDataFromPerBoneValues(HAND_FINGERS_POINTING_POSE);
        static const api::FRIKApi::HandPoseData holdingGun = makeHandPoseDataFromPerBoneValues(HAND_FINGERS_HOLDING_GUN_POSE);
        static const api::FRIKApi::HandPoseData holdingMelee = makeHandPoseDataFromPerBoneValues(HAND_FINGERS_HOLDING_MELEE_POSE);
        static const api::FRIKApi::HandPoseData offhandGrip = makeHandPoseDataFromPerBoneValues(OFFHAND_FINGERS_GRIP_POSE);
        static const api::FRIKApi::HandPoseData attaboy = makeHandPoseDataFromPerBoneValues(HAND_FINGERS_ATTABOY_HOLDING_POSE);
        static const api::FRIKApi::HandPoseData thumbsUp = [] {
            auto pose = makeHandPoseDataFromPerBoneValues(HAND_FINGERS_THUMBS_UP_POSE);
            pose.thumb.splay = 0.5f;
            return pose;
        }();

        switch (pose) {
        case api::FRIKApi::HandPoseKind::Fist:
            return &fist;
        case api::FRIKApi::HandPoseKind::Open:
            return &open;
        case api::FRIKApi::HandPoseKind::Pointing:
            return &pointing;
        case api::FRIKApi::HandPoseKind::HoldingWeapon:
            switch (hand_pose_weapon_policy::selectHoldingWeaponPose(g_frik.isWeaponDrawn(), g_frik.isMeleeWeaponDrawn())) {
            case hand_pose_weapon_policy::HoldingWeaponPoseSelection::Fist:
                return &fist;
            case hand_pose_weapon_policy::HoldingWeaponPoseSelection::HoldingMelee:
                return &holdingMelee;
            case hand_pose_weapon_policy::HoldingWeaponPoseSelection::HoldingGun:
                return &holdingGun;
            }
            return &holdingGun;
        case api::FRIKApi::HandPoseKind::OffhandGrip:
            return &offhandGrip;
        case api::FRIKApi::HandPoseKind::Attaboy:
            return &attaboy;
        case api::FRIKApi::HandPoseKind::ThumbsUp:
            return &thumbsUp;
        case api::FRIKApi::HandPoseKind::HoldingGun:
            return &holdingGun;
        case api::FRIKApi::HandPoseKind::HoldingMelee:
            return &holdingMelee;
        default:
            return nullptr;
        }
    }

    bool HandPoseManager::setTaggedPose(const std::string& tag, bool isLeft, int priority,
        api::FRIKApi::HandPoseKind poseType, float thumb, float index, float middle, float ring, float pinky)
    {
        const int idx = isLeft ? 1 : 0;
        auto& stack = _stacks[idx];

        // Find existing entry with same tag
        auto it = std::find_if(stack.begin(), stack.end(), [&](const HandPoseEntry& e) { return e.tag == tag; });

        HandPoseEntry entry = it != stack.end() ? *it : HandPoseEntry{};
        entry.tag = tag;
        entry.priority = priority;
        entry.poseType = poseType;
        entry.usesPerBone = false;
        entry.usesFullPoseData = false;
        entry.hasScalarPose = true;
        entry.sequence = ++_nextSequence;
        copyPoseDataToEntry(makeHandPoseDataFromFiveFingerValues(thumb, index, middle, ring, pinky), entry);

        if (it != stack.end()) {
            *it = entry;
        } else {
            stack.push_back(entry);
        }

        sortHandPoseStack(stack);

        logger::debug("[HandPoseManager] Set tag '{}' on {} hand, priority={}", tag, isLeft ? "left" : "right", priority);
        applyTopEntry(isLeft);
        return true;
    }

    bool HandPoseManager::setTaggedPose(const std::string& tag, bool isLeft, int priority, api::FRIKApi::HandPoseKind poseType)
    {
        if (poseType == api::FRIKApi::HandPoseKind::Unset) {
            clearTaggedPose(tag, isLeft);
            return true;
        }

        if (poseType == api::FRIKApi::HandPoseKind::Custom) {
            return false;
        }

        const auto* predefined = getPredefinedPoseData(poseType);
        if (!predefined) {
            return false;
        }
        return setTaggedPoseCustom(tag, isLeft, priority, *predefined, poseType);
    }

    bool HandPoseManager::setTaggedPoseCustom(
        const std::string& tag,
        bool isLeft,
        int priority,
        const api::FRIKApi::HandPoseData& poseData,
        api::FRIKApi::HandPoseKind poseType)
    {
        const int idx = isLeft ? 1 : 0;
        auto& stack = _stacks[idx];

        auto it = std::find_if(stack.begin(), stack.end(), [&](const HandPoseEntry& e) { return e.tag == tag; });

        HandPoseEntry entry = it != stack.end() ? *it : HandPoseEntry{};
        entry.tag = tag;
        entry.priority = priority;
        entry.poseType = poseType;
        entry.usesPerBone = true;
        entry.usesFullPoseData = true;
        entry.hasScalarPose = true;
        entry.sequence = ++_nextSequence;
        copyPoseDataToEntry(poseData, entry);

        if (it != stack.end()) {
            *it = entry;
        } else {
            stack.push_back(entry);
        }

        sortHandPoseStack(stack);

        logger::debug("[HandPoseManager] Set full pose tag '{}' on {} hand, priority={}", tag, isLeft ? "left" : "right", priority);
        applyTopEntry(isLeft);
        return true;
    }

    bool HandPoseManager::setTaggedPosePerBone(const std::string& tag, bool isLeft, int priority, const float* perBoneValues)
    {
        const int idx = isLeft ? 1 : 0;
        auto& stack = _stacks[idx];

        auto it = std::find_if(stack.begin(), stack.end(), [&](const HandPoseEntry& e) { return e.tag == tag; });

        HandPoseEntry entry = it != stack.end() ? *it : HandPoseEntry{};
        entry.tag = tag;
        entry.priority = priority;
        entry.poseType = api::FRIKApi::HandPoseKind::Custom;
        entry.usesPerBone = true;
        entry.usesFullPoseData = false;
        entry.hasScalarPose = true;
        std::memcpy(entry.perBoneFingers, perBoneValues, sizeof(float) * 15);
        entry.poseData = makeHandPoseDataFromPerBoneValues(perBoneValues);
        entry.sequence = ++_nextSequence;
        // Also fill the 5-finger summary for API queries
        entry.fingers[0] = perBoneValues[0];  // thumb
        entry.fingers[1] = perBoneValues[3];  // index
        entry.fingers[2] = perBoneValues[6];  // middle
        entry.fingers[3] = perBoneValues[9];  // ring
        entry.fingers[4] = perBoneValues[12]; // pinky

        if (it != stack.end()) {
            *it = entry;
        } else {
            stack.push_back(entry);
        }

        sortHandPoseStack(stack);

        logger::debug("[HandPoseManager] Set per-bone tag '{}' on {} hand, priority={}", tag, isLeft ? "left" : "right", priority);
        applyTopEntry(isLeft);
        return true;
    }

    bool HandPoseManager::setTaggedPoseLocalTransforms(
        const std::string& tag,
        bool isLeft,
        int priority,
        const api::FRIKApi::FingerLocalTransformOverride& overrideData)
    {
        const int idx = isLeft ? 1 : 0;
        auto& stack = _stacks[idx];

        auto it = std::find_if(stack.begin(), stack.end(), [&](const HandPoseEntry& e) { return e.tag == tag; });

        const std::uint16_t sanitizedMask = overrideData.enabledMask & 0x7FFF;
        const auto action = hand_pose_stack_policy::classifyLocalTransformUpdate(
            it != stack.end(),
            it != stack.end() && it->hasScalarPose,
            sanitizedMask);

        if (action == hand_pose_stack_policy::LocalTransformUpdateAction::RejectMissingScalarPose) {
            logger::warn("[HandPoseManager] Rejected local transform tag '{}' on {} hand without an active scalar pose, mask=0x{:04X}",
                tag, isLeft ? "left" : "right", sanitizedMask);
            return false;
        }

        if (action == hand_pose_stack_policy::LocalTransformUpdateAction::EraseTransformOnlyPose) {
            stack.erase(it);
            logger::debug("[HandPoseManager] Erased transform-only tag '{}' on {} hand", tag, isLeft ? "left" : "right");
            if (stack.empty()) {
                clearLegacyMaps(isLeft);
            } else {
                applyTopEntry(isLeft);
            }
            return true;
        }

        HandPoseEntry entry = *it;
        entry.tag = tag;
        entry.priority = priority;
        entry.poseType = api::FRIKApi::HandPoseKind::Custom;
        entry.sequence = ++_nextSequence;

        entry.localTransformMask = sanitizedMask;
        for (int i = 0; i < FINGERS_COUNT; i++) {
            entry.localTransforms[i] = overrideData.localTransforms[i];
        }

        *it = entry;

        sortHandPoseStack(stack);

        logger::debug("[HandPoseManager] Set local transform tag '{}' on {} hand, priority={}, mask=0x{:04X}",
            tag, isLeft ? "left" : "right", priority, entry.localTransformMask);
        applyTopEntry(isLeft);
        return true;
    }

    bool HandPoseManager::clearTaggedPose(const std::string& tag, bool isLeft)
    {
        const int idx = isLeft ? 1 : 0;
        auto& stack = _stacks[idx];

        auto it = std::find_if(stack.begin(), stack.end(), [&](const HandPoseEntry& e) { return e.tag == tag; });
        if (it == stack.end()) {
            return false;
        }

        stack.erase(it);
        logger::debug("[HandPoseManager] Cleared tag '{}' on {} hand, {} entries remain", tag, isLeft ? "left" : "right", stack.size());

        if (stack.empty()) {
            clearLegacyMaps(isLeft);
        } else {
            applyTopEntry(isLeft);
        }
        return true;
    }

    api::FRIKApi::HandPoseTagState HandPoseManager::getTagState(const std::string& tag, bool isLeft) const
    {
        const int idx = isLeft ? 1 : 0;
        const auto& stack = _stacks[idx];

        if (stack.empty()) {
            return api::FRIKApi::HandPoseTagState::None;
        }

        // Check if this tag exists
        auto it = std::find_if(stack.begin(), stack.end(), [&](const HandPoseEntry& e) { return e.tag == tag; });
        if (it == stack.end()) {
            return api::FRIKApi::HandPoseTagState::None;
        }

        // Top of stack (index 0) is active, rest are overridden
        if (it == stack.begin()) {
            return api::FRIKApi::HandPoseTagState::Active;
        }
        return api::FRIKApi::HandPoseTagState::Overriden;
    }

    api::FRIKApi::HandPoseKind HandPoseManager::getCurrentPose(bool isLeft) const
    {
        const int idx = isLeft ? 1 : 0;
        const auto& stack = _stacks[idx];
        if (stack.empty()) {
            return api::FRIKApi::HandPoseKind::Unset;
        }
        return stack.front().poseType;
    }

    bool HandPoseManager::hasActiveOverride(bool isLeft) const
    {
        const int idx = isLeft ? 1 : 0;
        return !_stacks[idx].empty();
    }

    void HandPoseManager::applyTopEntry(bool isLeft)
    {
        const int idx = isLeft ? 1 : 0;
        const auto& stack = _stacks[idx];
        if (stack.empty()) {
            clearLegacyMaps(isLeft);
            return;
        }

        const auto& top = stack.front();
        const auto* const fingersArray = isLeft ? LEFT_HAND_FINGERS : RIGHT_HAND_FINGERS;
        const std::size_t palmIndex = handPosePalmIndex(isLeft);
        handPosePalmPitch[palmIndex] = 0.0f;
        handPosePalmYaw[palmIndex] = 0.0f;
        handPosePalmHasControl[palmIndex] = false;

        if (top.usesFullPoseData && std::isfinite(top.poseData.palmPitch) && std::isfinite(top.poseData.palmYaw)) {
            handPosePalmPitch[palmIndex] = top.poseData.palmPitch;
            handPosePalmYaw[palmIndex] = top.poseData.palmYaw;
            handPosePalmHasControl[palmIndex] = top.poseData.palmPitch != 0.0f || top.poseData.palmYaw != 0.0f;
        }

        for (int i = 0; i < FINGERS_COUNT; i++) {
            handPapyrusHasControl[fingersArray[i]] = top.hasScalarPose;
            if (top.hasScalarPose) {
                handPapyrusPose[fingersArray[i]] = top.perBoneFingers[i];
            }

            const bool hasSplay = top.usesFullPoseData && top.hasScalarPose && i % 3 == 0;
            handPoseSplayHasControl[fingersArray[i]] = false;
            handPoseSplay[fingersArray[i]] = 0.0f;
            if (hasSplay) {
                const float splay = fingerSplayAt(top.poseData, i / 3);
                if (std::isfinite(splay)) {
                    handPoseSplay[fingersArray[i]] = splay;
                    handPoseSplayHasControl[fingersArray[i]] = true;
                }
            }

            const bool hasLocalTransform = (top.localTransformMask & (1U << i)) != 0;
            handLocalTransformHasControl[fingersArray[i]] = hasLocalTransform;
            if (hasLocalTransform) {
                handLocalTransformOverride[fingersArray[i]] = top.localTransforms[i];
            }
        }
    }

    void HandPoseManager::clearLegacyMaps(bool isLeft)
    {
        const auto* const fingersArray = isLeft ? LEFT_HAND_FINGERS : RIGHT_HAND_FINGERS;
        const std::size_t palmIndex = handPosePalmIndex(isLeft);
        handPosePalmPitch[palmIndex] = 0.0f;
        handPosePalmYaw[palmIndex] = 0.0f;
        handPosePalmHasControl[palmIndex] = false;
        for (int i = 0; i < FINGERS_COUNT; i++) {
            handPapyrusHasControl[fingersArray[i]] = false;
            handLocalTransformHasControl[fingersArray[i]] = false;
            handPoseSplayHasControl[fingersArray[i]] = false;
        }
    }
}
