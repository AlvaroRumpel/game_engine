#include "RenderQueue.h"
#include "../Renderer/Renderer.h"
#include "../Assets/Texture.h"
#include <algorithm>
#include <cstdint>

static std::uintptr_t TexKey(const Texture *t)
{
    return reinterpret_cast<std::uintptr_t>(t);
}

void RenderQueue::nextFrame(std::uint64_t frameIndex)
{
    stats_ = RenderStats{};
    stats_.frameIndex = frameIndex;
}

void RenderQueue::clear()
{
    cmds_.clear();
    spriteBatches_.clear();
}

void RenderQueue::submit(const RenderCommand &cmd)
{
    cmds_.push_back(cmd);
    stats_.commandsSubmitted++;
}

void RenderQueue::compileBatches()
{
    // Pré-condição: cmds_ já ordenado.
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

        current->sprites.push_back(SpriteInstance{c.x, c.y, c.scale});
    }
}

void RenderQueue::flush(Renderer &renderer)
{
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

    // 1) desenha rects (não batcheamos agora)
    for (const auto &c : cmds_)
    {
        if (c.type != RenderCommandType::Rect)
            continue;

        renderer.drawRect(c.x, c.y, c.w, c.h, c.r, c.g, c.b, c.a);
        stats_.rectDraws++;
    }

    // 2) compila batches de sprite e executa
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

        // Por enquanto (SDL): ainda faz N RenderCopy.
        // Em OpenGL, isso vira 1 draw call por batch.
        for (const auto &inst : batch.sprites)
        {
            renderer.drawTexture(*batch.texture, inst.x, inst.y, inst.scale);
            stats_.spriteDraws++;
        }
    }
}
