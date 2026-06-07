#pragma once

#include <deque>

#include "common/CommonUtils.h"

namespace frik
{
    class SmoothMovementVR
    {
    public:
        SmoothMovementVR()
        {
            QueryPerformanceFrequency(&_hpcFrequency);
            QueryPerformanceCounter(&_prevTime);
        }

        void onFrameUpdate();

        // --- ROCK integration getters ---

        /// Get the current smoothed player position (for player-space simulation)
        RE::NiPoint3 getSmoothedPosition() const { return _smoothedPos; }

        /// Whether the player is currently stationary (optimization hint)
        bool isMoving() const { return !_notMoving; }

        /// Get the per-frame time delta used by smooth movement
        float getFrameTime() const { return _frameTime; }

    private:
        RE::NiPoint3 smoothedValue(const RE::NiPoint3& curPos, const RE::NiPoint3& prevPos);

        bool _notMoving = false;
        std::deque<RE::NiPoint3> _lastPositions;
        RE::NiPoint3 _smoothedPos;
        float _lastAppliedLocalX = 0;
        float _lastAppliedLocalY = 0;

        LARGE_INTEGER _hpcFrequency;
        LARGE_INTEGER _prevTime;
        float _frameTime = 0;
    };
}
