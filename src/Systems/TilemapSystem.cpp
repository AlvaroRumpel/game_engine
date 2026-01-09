#include "TilemapSystem.h"
#include "../Engine/Engine.h"
#include "../World/Scene.h"
#include <algorithm>
#include <cmath>

struct TileColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

static TileColor ColorForTile(int id)
{
    static const TileColor palette[] = {
        {40, 40, 50, 255},
        {70, 120, 200, 255},
        {60, 160, 90, 255},
        {160, 90, 70, 255},
        {140, 140, 80, 255}};

    if (id < 0)
        return {0, 0, 0, 0};
    int index = id % (int)(sizeof(palette) / sizeof(palette[0]));
    return palette[index];
}

void TilemapSystem::render(Engine &engine, const Scene &scene)
{
    auto &q = engine.renderQueue();

    float wx0 = 0.0f;
    float wy0 = 0.0f;
    float wx1 = 0.0f;
    float wy1 = 0.0f;
    engine.screenToWorld(0.0f, 0.0f, wx0, wy0);
    engine.screenToWorld((float)engine.screenWidth(), (float)engine.screenHeight(), wx1, wy1);

    float minX = std::min(wx0, wx1);
    float maxX = std::max(wx0, wx1);
    float minY = std::min(wy0, wy1);
    float maxY = std::max(wy0, wy1);

    for (const auto &map : scene.tilemaps())
    {
        if (map.width <= 0 || map.height <= 0 || map.tileSize <= 0)
            continue;

        float inv = 1.0f / (float)map.tileSize;
        int minTx = (int)std::floor((minX - map.originX) * inv);
        int maxTx = (int)std::floor((maxX - map.originX) * inv);
        int minTy = (int)std::floor((minY - map.originY) * inv);
        int maxTy = (int)std::floor((maxY - map.originY) * inv);

        minTx = std::max(minTx, 0);
        minTy = std::max(minTy, 0);
        maxTx = std::min(maxTx, map.width - 1);
        maxTy = std::min(maxTy, map.height - 1);

        for (int ty = minTy; ty <= maxTy; ++ty)
        {
            for (int tx = minTx; tx <= maxTx; ++tx)
            {
                int tileId = map.get(tx, ty);
                if (tileId < 0)
                    continue;

                TileColor c = ColorForTile(tileId);
                RenderCommand cmd;
                cmd.type = RenderCommandType::Rect;
                cmd.layer = -10;
                cmd.x = map.originX + (tx * map.tileSize);
                cmd.y = map.originY + (ty * map.tileSize);
                cmd.w = map.tileSize;
                cmd.h = map.tileSize;
                cmd.r = c.r;
                cmd.g = c.g;
                cmd.b = c.b;
                cmd.a = c.a;
                q.submit(cmd);
            }
        }
    }
}
