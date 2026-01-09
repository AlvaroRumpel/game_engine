#pragma once
#include <vector>
#include "RenderCommand.h"
#include "RenderStats.h"
#include "RenderBatch.h"

class Renderer;

class RenderQueue
{
public:
    void clear();
    void submit(const RenderCommand &cmd);

    void nextFrame(std::uint64_t frameIndex);

    // novo: compila e executa
    void flush(Renderer &renderer);

    const RenderStats &stats() const { return stats_; }

private:
    void compileBatches();

private:
    std::vector<RenderCommand> cmds_;
    std::vector<RenderBatch> spriteBatches_;
    RenderStats stats_;
};
