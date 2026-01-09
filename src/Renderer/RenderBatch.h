#pragma once
#include <cstdint>
#include <vector>

class Texture;

struct SpriteInstance
{
    float x = 0, y = 0;
    float scale = 1.0f;
    bool useSrcRect = false;
    int srcX = 0;
    int srcY = 0;
    int srcW = 0;
    int srcH = 0;
    float rotationDeg = 0.0f;
};

struct RenderBatch
{
    int layer = 0;
    const Texture *texture = nullptr;
    std::vector<SpriteInstance> sprites;
};
