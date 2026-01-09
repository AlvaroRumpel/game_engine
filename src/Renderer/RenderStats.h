#pragma once
#include <cstdint>

struct RenderStats
{
    std::uint64_t frameIndex = 0;

    std::uint32_t commandsSubmitted = 0;
    std::uint32_t rectDraws = 0;
    std::uint32_t spriteDraws = 0;

    std::uint32_t textureBindsEstimated = 0; // heurística p/ backends
    std::uint32_t spriteBatches = 0;
};
