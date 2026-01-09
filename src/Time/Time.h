#pragma once
#include <cstdint>

class Time
{
public:
    void reset();

    // Engine: chamada 1x por frame
    void beginFrame(float rawDeltaSeconds);

    // Engine: chamada a cada "fixed step" executado
    void consumeFixedStep();

    // API (Game/Engine)
    float deltaTime() const { return deltaTime_; }           // dt do frame
    float fixedDelta() const { return fixedDelta_; }         // dt fixo
    std::uint64_t frameCount() const { return frameCount_; } // frames renderizados
    std::uint64_t fixedStepCount() const { return fixedStepCount_; }

    float timeSinceStart() const { return timeSinceStart_; } // segundos (aprox.)
    float accumulator() const { return accumulator_; }       // segundos acumulados p/ fixed

    // Config
    void setFixedDelta(float seconds) { fixedDelta_ = seconds; }
    void setMaxDelta(float seconds) { maxDelta_ = seconds; } // clamp anti “alt-tab”
    void setMaxFixedStepsPerFrame(int steps) { maxFixedStepsPerFrame_ = steps; }

    int maxFixedStepsPerFrame() const { return maxFixedStepsPerFrame_; }

private:
    float deltaTime_ = 0.0f;
    float fixedDelta_ = 1.0f / 60.0f;

    float maxDelta_ = 0.1f; // 100ms
    int maxFixedStepsPerFrame_ = 5;

    float accumulator_ = 0.0f;

    std::uint64_t frameCount_ = 0;
    std::uint64_t fixedStepCount_ = 0;

    float timeSinceStart_ = 0.0f;
};
