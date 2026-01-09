#include "Time.h"

void Time::reset()
{
    deltaTime_ = 0.0f;
    unscaledDeltaTime_ = 0.0f;
    accumulator_ = 0.0f;
    frameCount_ = 0;
    fixedStepCount_ = 0;
    timeSinceStart_ = 0.0f;
    fps_ = 0.0f;
    fpsAccumTime_ = 0.0f;
    fpsAccumFrames_ = 0;
}

void Time::beginFrame(float rawDeltaSeconds)
{
    if (rawDeltaSeconds < 0.0f)
        rawDeltaSeconds = 0.0f;

    unscaledDeltaTime_ = rawDeltaSeconds;

    // clamp para dt escalado
    float clamped = rawDeltaSeconds;
    if (clamped > maxDelta_)
        clamped = maxDelta_;

    deltaTime_ = clamped;
    timeSinceStart_ += deltaTime_;
    accumulator_ += deltaTime_;
    frameCount_++;

    fpsAccumTime_ += unscaledDeltaTime_;
    fpsAccumFrames_++;
    if (fpsAccumTime_ >= 0.5f)
    {
        fps_ = (fpsAccumTime_ > 0.0f) ? (float)fpsAccumFrames_ / fpsAccumTime_ : 0.0f;
        fpsAccumTime_ = 0.0f;
        fpsAccumFrames_ = 0;
    }
}

void Time::consumeFixedStep()
{
    accumulator_ -= fixedDelta_;
    fixedStepCount_++;
}
