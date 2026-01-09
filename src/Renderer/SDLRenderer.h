#pragma once
#include "Renderer.h"
#include "../Engine/Camera2D.h"

struct SDL_Renderer;

class Texture;

class SDLRenderer final : public Renderer
{
public:
    explicit SDLRenderer(SDL_Renderer *sdlRenderer);

    void beginFrame() override;
    void endFrame() override;

    void drawRect(float x, float y, int w, int h,
                  unsigned char r, unsigned char g, unsigned char b, unsigned char a) override;

    void drawTexture(const Texture &tex, float x, float y, float scale = 1.0f) override;

    void drawText(const Font &font, const std::string &text,
                  float x, float y,
                  unsigned char r, unsigned char g, unsigned char b, unsigned char a) override;

    // expor para o AssetManager poder criar SDL_Texture
    SDL_Renderer *native() const { return r_; }
    void setCamera(const Camera2D &cam, int screenW, int screenH) override;

private:
    SDL_Renderer *r_ = nullptr;
    Camera2D cam_{};
    int screenW_ = 800;
    int screenH_ = 600;

    float worldToScreenX(float worldX) const;
    float worldToScreenY(float worldY) const;
};
