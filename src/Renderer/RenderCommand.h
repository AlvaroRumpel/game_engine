#pragma once
#include <cstdint>

class Texture;

enum class RenderCommandType : std::uint8_t
{
    Rect,
    Sprite
};

struct RenderCommand
{
    RenderCommandType type;

    int layer = 0; // ordenação

    // comum
    float x = 0, y = 0;

    // rect
    int w = 0, h = 0;
    std::uint8_t r = 255, g = 255, b = 255, a = 255;

    // sprite
    const Texture *texture = nullptr; // ponteiro não-dono (AssetManager mantém vivo)
    float scale = 1.0f;
};
