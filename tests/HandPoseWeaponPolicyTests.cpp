#include "skeleton/HandPoseWeaponPolicy.h"

#include <cstdio>

namespace
{
    bool expectSelection(
        const char* name,
        frik::hand_pose_weapon_policy::HoldingWeaponPoseSelection actual,
        frik::hand_pose_weapon_policy::HoldingWeaponPoseSelection expected)
    {
        if (actual != expected) {
            std::printf("%s expected %d got %d\n", name, static_cast<int>(expected), static_cast<int>(actual));
            return false;
        }
        return true;
    }
}

int main()
{
    using frik::hand_pose_weapon_policy::HoldingWeaponPoseSelection;
    using frik::hand_pose_weapon_policy::selectHoldingWeaponPose;

    bool ok = true;
    ok &= expectSelection("unarmed selects fist", selectHoldingWeaponPose(false, false), HoldingWeaponPoseSelection::Fist);
    ok &= expectSelection("unarmed ignores stale melee flag", selectHoldingWeaponPose(false, true), HoldingWeaponPoseSelection::Fist);
    ok &= expectSelection("drawn gun selects gun grip", selectHoldingWeaponPose(true, false), HoldingWeaponPoseSelection::HoldingGun);
    ok &= expectSelection("drawn melee selects melee grip", selectHoldingWeaponPose(true, true), HoldingWeaponPoseSelection::HoldingMelee);
    return ok ? 0 : 1;
}

