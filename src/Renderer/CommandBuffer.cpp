#include "CommandBuffer.h"
#include "../Assets/Texture.h"
#include <algorithm>
#include <cstdint>

static std::uintptr_t TexKey(const Texture *t)
{
    return reinterpret_cast<std::uintptr_t>(t);
}

void CommandBuffer::nextFrame(std::uint64_t frameIndex)
{
    stats_ = RenderStats{};
    stats_.frameIndex = frameIndex;
    finalized_ = false;
}

void CommandBuffer::clear()
{
    cmds_.clear();
    spriteBatches_.clear();
    finalized_ = false;
}

void CommandBuffer::submit(const RenderCommand &cmd)
{
    cmds_.push_back(cmd);
    stats_.commandsSubmitted++;
    finalized_ = false;
}

void CommandBuffer::compileBatches()
{
    spriteBatches_.clear();

    RenderBatch *current = nullptr;

    for (const auto &c : cmds_)
    {
        if (c.type != RenderCommandType::Sprite || !c.texture)
            continue;

        bool needNew =
            (current == nullptr) ||
            (current->layer != c.layer) ||
            (current->texture != c.texture);

        if (needNew)
        {
            spriteBatches_.push_back(RenderBatch{});
            current = &spriteBatches_.back();
            current->layer = c.layer;
            current->texture = c.texture;
            stats_.spriteBatches++;
        }

        SpriteInstance inst;
        inst.x = c.x;
        inst.y = c.y;
        inst.scale = c.scale;
        inst.useSrcRect = c.useSrcRect;
        inst.srcX = c.srcX;
        inst.srcY = c.srcY;
        inst.srcW = c.srcW;
        inst.srcH = c.srcH;
        inst.rotationDeg = c.rotationDeg;
        current->sprites.push_back(inst);
    }
}

void CommandBuffer::finalize()
{
    if (finalized_)
        return;

    // Ordena: layer -> type -> texture
    std::sort(cmds_.begin(), cmds_.end(),
              [](const RenderCommand &a, const RenderCommand &b)
              {
                  if (a.layer != b.layer)
                      return a.layer < b.layer;
                  if (a.type != b.type)
                      return (int)a.type < (int)b.type;
                  return TexKey(a.texture) < TexKey(b.texture);
              });

    stats_.rectDraws = 0;
    stats_.spriteDraws = 0;
    stats_.textureBindsEstimated = 0;
    stats_.spriteBatches = 0;

    for (const auto &c : cmds_)
    {
        if (c.type == RenderCommandType::Rect)
            stats_.rectDraws++;
    }

    compileBatches();

    const Texture *lastTex = nullptr;
    for (const auto &batch : spriteBatches_)
    {
        if (!batch.texture)
            continue;
        if (batch.texture != lastTex)
        {
            stats_.textureBindsEstimated++;
            lastTex = batch.texture;
        }
        stats_.spriteDraws += (std::uint32_t)batch.sprites.size();
    }

    finalized_ = true;
}
