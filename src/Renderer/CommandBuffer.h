#pragma once
#include <vector>
#include "RenderCommand.h"
#include "RenderStats.h"
#include "RenderBatch.h"

class CommandBuffer
{
public:
    void clear();
    void submit(const RenderCommand &cmd);

    void nextFrame(std::uint64_t frameIndex);
    void finalize();

    const RenderStats &stats() const { return stats_; }
    const std::vector<RenderCommand> &commands() const { return cmds_; }
    const std::vector<RenderBatch> &spriteBatches() const { return spriteBatches_; }

private:
    void compileBatches();

private:
    std::vector<RenderCommand> cmds_;
    std::vector<RenderBatch> spriteBatches_;
    RenderStats stats_;
    bool finalized_ = false;
};
