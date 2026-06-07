#include "skeleton/HandPoseLocalTransformPolicy.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    bool expectBool(const char* name, bool actual, bool expected)
    {
        if (actual != expected) {
            std::printf("%s expected %s got %s\n", name, expected ? "true" : "false", actual ? "true" : "false");
            return false;
        }
        return true;
    }

    bool expectFloat(const char* name, float actual, float expected)
    {
        if (std::fabs(actual - expected) > 0.0001f) {
            std::printf("%s expected %.4f got %.4f\n", name, expected, actual);
            return false;
        }
        return true;
    }

    bool expectString(const char* name, const char* actual, const char* expected)
    {
        if (!actual || std::strcmp(actual, expected) != 0) {
            std::printf("%s expected %s got %s\n", name, expected, actual ? actual : "(null)");
            return false;
        }
        return true;
    }
}

int main()
{
    bool ok = true;
    using namespace frik::hand_pose_local_transform_policy;

    ok &= expectBool("non-null per-bone values can build baseline",
        shouldBuildBaselineLocalTransforms(reinterpret_cast<const float*>(0x1)), true);
    ok &= expectBool("missing per-bone values reject baseline",
        shouldBuildBaselineLocalTransforms(nullptr), false);

    ok &= expectFloat("pose value clamps below FRIK blend range",
        sanitizePoseBlendValue(-5.0f), -1.0f);
    ok &= expectFloat("pose value clamps above FRIK blend range",
        sanitizePoseBlendValue(8.0f), 2.0f);
    ok &= expectFloat("non-finite pose value falls back to open hand",
        sanitizePoseBlendValue(std::nanf("")), 1.0f);

    ok &= expectString("right thumb proximal bone name",
        fingerLocalTransformBoneName(false, 0), "RArm_Finger11");
    ok &= expectString("left pinky distal bone name",
        fingerLocalTransformBoneName(true, 14), "LArm_Finger53");
    ok &= expectBool("invalid bone index returns null",
        fingerLocalTransformBoneName(true, 15) == nullptr, true);

    return ok ? 0 : 1;
}
