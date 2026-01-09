#pragma once
#include <memory>

struct Transform
{
    float x = 0.0f;
    float y = 0.0f;
};

struct RectRender
{
    int w = 32;
    int h = 32;
    unsigned char r = 255, g = 255, b = 255, a = 255;
    bool enabled = true; // debug/fallback
};

class Texture;

struct SpriteRender
{
    std::shared_ptr<Texture> texture;
    float scale = 1.0f;
    bool enabled = false;
};

struct Entity
{
    int id = 0;
    bool isSolid = true;

    int renderLayer = 0;

    Transform transform;
    RectRender rect;
    SpriteRender sprite;
};
