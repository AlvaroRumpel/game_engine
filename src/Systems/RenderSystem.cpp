#include "RenderSystem.h"
#include "../Engine/Engine.h"
#include "../World/Scene.h"

void RenderSystem::render(Engine &engine, const Scene &scene)
{
    auto &q = engine.renderQueue();

    for (const auto &e : scene.entities())
    {
        // Sprite
        if (e.sprite.enabled && e.sprite.texture)
        {
            RenderCommand cmd;
            cmd.type = RenderCommandType::Sprite;
            cmd.layer = e.renderLayer; // depois vira componente
            cmd.x = e.transform.x;
            cmd.y = e.transform.y;
            cmd.texture = e.sprite.texture.get();
            cmd.scale = e.sprite.scale;
            q.submit(cmd);
            continue;
        }

        // Rect fallback
        if (e.rect.enabled)
        {
            RenderCommand cmd;
            cmd.type = RenderCommandType::Rect;
            cmd.layer = e.renderLayer;
            cmd.x = e.transform.x;
            cmd.y = e.transform.y;
            cmd.w = e.rect.w;
            cmd.h = e.rect.h;
            cmd.r = e.rect.r;
            cmd.g = e.rect.g;
            cmd.b = e.rect.b;
            cmd.a = e.rect.a;
            q.submit(cmd);
        }
    }
}
