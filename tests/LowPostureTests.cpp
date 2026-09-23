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

TEST_CASE("Floor orientation preserves belly and upright head movements until a clear side or back cue", "[low-posture]")
{
    REQUIRE_FALSE(hasOrientationCue(0.0f, 1.0f, 0.0f, 1.0f));
    REQUIRE_FALSE(hasOrientationCue(0.2f, 1.0f, 0.0f, 1.0f));
    REQUIRE_FALSE(hasOrientationCue(1.0f, -0.9f, 0.0f, 0.9f));
    REQUIRE_FALSE(hasOrientationCue(1.0f, 0.4f, 0.0f, 0.4f));
    REQUIRE_FALSE(hasOrientationCue(1.0f, 0.0f, 0.0f, 0.0f));
    REQUIRE(hasOrientationCue(1.0f, 1.0f, 0.0f, 1.0f));
    REQUIRE(hasOrientationCue(1.0f, 0.0f, 1.0f, 1.0f));
    REQUIRE(hasOrientationCue(1.0f, 0.0f, -1.0f, 1.0f));
    REQUIRE_FALSE(hasOrientationCue(1.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f, 1.0f));
}

TEST_CASE("Floor roll leaves head-turn room at belly and back and crosses both sides continuously", "[low-posture]")
{
    constexpr float rad = std::numbers::pi_v<float> / 180.0f;
    for (const float degrees : { -40.0f, 0.0f, 40.0f }) {
        REQUIRE(*bodyRoll(std::sin(degrees * rad), std::cos(degrees * rad)) == Catch::Approx(0.0f).margin(0.00001f));
    }
    for (const float degrees : { -180.0f, -140.0f, 140.0f, 180.0f }) {
        REQUIRE(std::abs(*bodyRoll(std::sin(degrees * rad), std::cos(degrees * rad))) == Catch::Approx(std::numbers::pi_v<float>));
    }
    REQUIRE(*bodyRoll(1.0f, 0.0f) == Catch::Approx(std::numbers::pi_v<float> / 2));
    REQUIRE(*bodyRoll(-1.0f, 0.0f) == Catch::Approx(-std::numbers::pi_v<float> / 2));
    float previous = 0.0f;
    for (int degrees = 0; degrees < 180; ++degrees) {
        const float value = *bodyRoll(std::sin(degrees * rad), std::cos(degrees * rad));
        REQUIRE(value >= previous);
        REQUIRE(value - previous < 0.053f);
        previous = value;
    }
    REQUIRE_FALSE(bodyRoll(0.0f, 0.0f)); // looking along the body: preserve the previous orientation, do not invent a roll
    REQUIRE_FALSE(bodyRoll(std::numeric_limits<float>::quiet_NaN(), 1.0f));
}

TEST_CASE("Floor frames distinguish rolling over from reclining backward without mirroring shoulders", "[low-posture]")
{
    const auto prone = *floorFrame(0, 1, 0);
    REQUIRE(prone.right[0] == Catch::Approx(1));
    REQUIRE(prone.front[2] == Catch::Approx(-1));
    const auto rolledBack = *floorFrame(0, 1, std::numbers::pi_v<float>);
    REQUIRE(rolledBack.right[0] == Catch::Approx(-1));
    REQUIRE(rolledBack.front[2] == Catch::Approx(1));
    REQUIRE(rolledBack.headward[1] == Catch::Approx(1));
    const auto reclinedBack = *floorFrame(0, -1, std::numbers::pi_v<float>);
    REQUIRE(reclinedBack.right[0] == Catch::Approx(1));
    REQUIRE(reclinedBack.front[2] == Catch::Approx(1));
    REQUIRE(reclinedBack.headward[1] == Catch::Approx(-1));
    REQUIRE_FALSE(floorFrame(0, 0, 0));
    REQUIRE_FALSE(floorFrame(1, 0, std::numeric_limits<float>::infinity()));
}

TEST_CASE("Floor frame rotations preserve limb geometry and body-relative projections", "[low-posture]")
{
    const auto dot = [](const auto& a, const auto& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    constexpr std::array<float, 3> wristInBody{ 12, 30, -8 };
    for (int yaw = -180; yaw <= 180; yaw += 30) {
        for (int roll = -180; roll <= 180; roll += 30) {
            const float yawRadians = yaw * std::numbers::pi_v<float> / 180.0f;
            const auto f = *floorFrame(std::sin(yawRadians), std::cos(yawRadians), roll * std::numbers::pi_v<float> / 180.0f);
            const std::array axes{ f.right, f.front, f.headward };
            std::array<float, 3> worldWrist{};
            for (int i = 0; i < 3; ++i) {
                REQUIRE(dot(axes[i], axes[i]) == Catch::Approx(1));
                REQUIRE(dot(axes[i], axes[(i + 1) % 3]) == Catch::Approx(0).margin(0.000001f));
                for (int j = 0; j < 3; ++j) {
                    worldWrist[j] += axes[i][j] * wristInBody[i];
                }
            }
            for (int i = 0; i < 3; ++i) {
                REQUIRE(dot(worldWrist, axes[i]) == Catch::Approx(wristInBody[i]).margin(0.00001f));
            }
            REQUIRE(dot(worldWrist, worldWrist) == Catch::Approx(dot(wristInBody, wristInBody)));
            const std::array cross{ f.right[1] * f.front[2] - f.right[2] * f.front[1],
                f.right[2] * f.front[0] - f.right[0] * f.front[2], f.right[0] * f.front[1] - f.right[1] * f.front[0] };
            REQUIRE(dot(cross, f.headward) == Catch::Approx(1)); // proper rotation, never a left/right reflection
        }
    }
}

TEST_CASE("Floor orientation transition has the same response at different frame rates", "[low-posture]")
{
    const auto remaining = [](const int hz) {
        float value = 1.0f;
        for (int i = 0; i < hz; ++i) {
            value *= 1.0f - orientationMoveFraction(1.0f / hz);
        }
        return value;
    };
    REQUIRE(remaining(45) == Catch::Approx(remaining(90)).margin(0.000001f));
    REQUIRE(remaining(120) == Catch::Approx(remaining(90)).margin(0.000001f));
    REQUIRE(orientationMoveFraction(0) == 0.0f);
    REQUIRE(orientationMoveFraction(-1) == 0.0f);
    REQUIRE(orientationMoveFraction(std::numeric_limits<float>::infinity()) == 0.0f);
}
