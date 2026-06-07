#include "skeleton/HandPoseStackPolicy.h"

#include <cstdio>

namespace
{
    bool expectAction(
        const char* name,
        frik::hand_pose_stack_policy::LocalTransformUpdateAction actual,
        frik::hand_pose_stack_policy::LocalTransformUpdateAction expected)
    {
        if (actual != expected) {
            std::printf("%s expected %d got %d\n", name, static_cast<int>(expected), static_cast<int>(actual));
            return false;
        }
        return true;
    }

    bool expectBool(const char* name, bool actual, bool expected)
    {
        if (actual != expected) {
            std::printf("%s expected %s got %s\n", name, expected ? "true" : "false", actual ? "true" : "false");
            return false;
        }
        return true;
    }
}

int main()
{
    bool ok = true;
    using frik::hand_pose_stack_policy::LocalTransformUpdateAction;
    ok &= expectAction("missing local transform tag rejected",
        frik::hand_pose_stack_policy::classifyLocalTransformUpdate(false, false, 0x0007),
        LocalTransformUpdateAction::RejectMissingScalarPose);
    ok &= expectAction("missing zero-mask local transform tag rejected",
        frik::hand_pose_stack_policy::classifyLocalTransformUpdate(false, false, 0x0000),
        LocalTransformUpdateAction::RejectMissingScalarPose);
    ok &= expectAction("scalar tag accepts local transform mask",
        frik::hand_pose_stack_policy::classifyLocalTransformUpdate(true, true, 0x0007),
        LocalTransformUpdateAction::ApplyToScalarPose);
    ok &= expectAction("scalar tag accepts local transform clear",
        frik::hand_pose_stack_policy::classifyLocalTransformUpdate(true, true, 0x0000),
        LocalTransformUpdateAction::ApplyToScalarPose);
    ok &= expectAction("legacy transform-only zero mask clears entry",
        frik::hand_pose_stack_policy::classifyLocalTransformUpdate(true, false, 0x0000),
        LocalTransformUpdateAction::EraseTransformOnlyPose);
    ok &= expectAction("legacy transform-only nonzero mask rejected",
        frik::hand_pose_stack_policy::classifyLocalTransformUpdate(true, false, 0x0007),
        LocalTransformUpdateAction::RejectMissingScalarPose);
    ok &= expectBool("higher priority sorts first",
        frik::hand_pose_stack_policy::sortsBefore(70, 1, 50, 99), true);
    ok &= expectBool("lower priority sorts after",
        frik::hand_pose_stack_policy::sortsBefore(50, 99, 70, 1), false);
    ok &= expectBool("newer same-priority sequence sorts first",
        frik::hand_pose_stack_policy::sortsBefore(50, 2, 50, 1), true);
    ok &= expectBool("older same-priority sequence sorts after",
        frik::hand_pose_stack_policy::sortsBefore(50, 1, 50, 2), false);
    return ok ? 0 : 1;
}
