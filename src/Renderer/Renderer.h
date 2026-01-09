#pragma once
#include <string>
#include "../Engine/Camera2D.h"

class Texture;
class Font;
class Engine;
class CommandBuffer;

struct TextureRegion
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

class Renderer
{
public:
    virtual ~Renderer() = default;

    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;

    virtual void drawRect(float x, float y, int w, int h,
                          unsigned char r, unsigned char g, unsigned char b, unsigned char a) = 0;

    virtual void drawTexture(const Texture &tex, float x, float y, float scale,
                             const TextureRegion *src, float rotationDeg) = 0;

    // novo: texto UTF-8
    virtual void drawText(const Font &font, const std::string &text,
                          float x, float y,
                          unsigned char r, unsigned char g, unsigned char b, unsigned char a) = 0;

    virtual void setCamera(const Camera2D &cam, int screenW, int screenH) = 0;
    virtual void submit(const CommandBuffer &cmds) = 0;
};
