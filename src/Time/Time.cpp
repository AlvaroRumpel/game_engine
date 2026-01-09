#include "Time.h"

void Time::reset()
{
    deltaTime_ = 0.0f;
    accumulator_ = 0.0f;
    frameCount_ = 0;
    fixedStepCount_ = 0;
    timeSinceStart_ = 0.0f;
}

void Time::beginFrame(float rawDeltaSeconds)
{
    // clamp
    if (rawDeltaSeconds < 0.0f)
        rawDeltaSeconds = 0.0f;
    if (rawDeltaSeconds > maxDelta_)
        rawDeltaSeconds = maxDelta_;

    deltaTime_ = rawDeltaSeconds;
    timeSinceStart_ += deltaTime_;
    accumulator_ += deltaTime_;
    frameCount_++;
}

void Time::consumeFixedStep()
{
    accumulator_ -= fixedDelta_;
    fixedStepCount_++;
}
