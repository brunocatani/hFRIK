#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "skeleton/LowPostureMath.h"

using namespace frik::lowposture;

TEST_CASE("Low posture joins crouch continuously and reaches prone near the physical floor", "[low-posture]")
{
    constexpr float standing = 1.8f;
    REQUIRE(*blendFromHeight(standing, standing) == 0.0f);
    REQUIRE(*blendFromHeight(standing * 0.40f, standing) == Catch::Approx(0.0f).margin(0.00001f));
    REQUIRE(*blendFromHeight(standing * 0.31f, standing) == Catch::Approx(0.5f));
    REQUIRE(*blendFromHeight(standing * 0.22f, standing) == Catch::Approx(1.0f));
    REQUIRE(*blendFromHeight(0.0f, standing) == 1.0f);
    float previous = 1.0f;
    for (int step = 0; step <= 1000; ++step) {
        const float height = standing * step / 1000.0f;
        const float blend = *blendFromHeight(height, standing);
        REQUIRE(blend <= previous + 0.000001f);
        REQUIRE(previous - blend < 0.009f);
        REQUIRE(blend == Catch::Approx(*blendFromHeight(height * 1.25f, standing * 1.25f)).margin(0.00001f));
        previous = blend;
    }
}

TEST_CASE("Low posture rejects invalid tracking/calibration instead of interpreting it as the floor", "[low-posture]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (const float invalid : { nan, inf, -1.0f }) {
        REQUIRE_FALSE(blendFromHeight(invalid, 1.8f));
        REQUIRE_FALSE(blendFromHeight(0.3f, invalid));
    }
    REQUIRE_FALSE(blendFromHeight(0.3f, 0.0f));
}

TEST_CASE("Low posture holds body heading through wraparound without a long-way spin", "[low-posture]")
{
    constexpr float rad = std::numbers::pi_v<float> / 180.0f;
    REQUIRE(blendHeading(179 * rad, -179 * rad, 0.5f) == Catch::Approx(std::numbers::pi_v<float>));
    REQUIRE(blendHeading(0.8f, 0.2f, 0.0f) == Catch::Approx(0.8f));
    REQUIRE(blendHeading(0.8f, 0.2f, 1.0f) == Catch::Approx(0.2f));
}

TEST_CASE("Hip projection preserves ordinary crouch and stays finite across horizontal", "[low-posture]")
{
    const std::array<float, 3> neckToHip{ 2.0f, 5.0f, -50.0f };
    const std::array<float, 3> backward{ 0.0f, -1.0f, 0.0f };
    for (int degrees = 0; degrees <= 80; ++degrees) {
        const float pitch = degrees * std::numbers::pi_v<float> / 180.0f;
        const auto direction = hipDirection(neckToHip, backward, pitch);
        REQUIRE(direction);
        auto original = neckToHip;
        original[1] -= std::tan(pitch) * std::hypot(neckToHip[0], neckToHip[1], neckToHip[2]);
        const float length = std::hypot(original[0], original[1], original[2]);
        for (std::size_t i = 0; i < original.size(); ++i) {
            REQUIRE((*direction)[i] == Catch::Approx(original[i] / length).margin(0.000001f));
        }
    }
    for (const float degrees : { 89.999f, 90.0f, 90.001f }) {
        const auto direction = hipDirection(neckToHip, backward, degrees * std::numbers::pi_v<float> / 180.0f);
        REQUIRE(direction);
        REQUIRE((*direction)[1] == Catch::Approx(-1.0f).margin(0.000001f));
        REQUIRE(std::abs((*direction)[2]) < 0.0001f);
    }
    REQUIRE_FALSE(hipDirection({ 0, 0, 0 }, backward, 0.0f));
    REQUIRE_FALSE(hipDirection(neckToHip, { 0, 0, 0 }, 0.0f));
    REQUIRE_FALSE(hipDirection(neckToHip, backward, std::numeric_limits<float>::quiet_NaN()));
}
