#pragma once

#include <cstdint>

namespace frik::hand_pose_weapon_policy
{
    enum class HoldingWeaponPoseSelection : std::uint8_t
    {
        Fist,
        HoldingGun,
        HoldingMelee,
    };

    [[nodiscard]] constexpr HoldingWeaponPoseSelection selectHoldingWeaponPose(
        const bool weaponDrawn,
        const bool meleeWeaponDrawn)
    {
        if (!weaponDrawn) {
            return HoldingWeaponPoseSelection::Fist;
        }
        return meleeWeaponDrawn ? HoldingWeaponPoseSelection::HoldingMelee : HoldingWeaponPoseSelection::HoldingGun;
    }
}

