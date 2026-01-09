#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include "Renderer.h"
#include "../Engine/Camera2D.h"

struct SDL_Renderer;
struct SDL_Texture;

class Texture;
class Font;

class SDLRenderer final : public Renderer
{
public:
    explicit SDLRenderer(SDL_Renderer *sdlRenderer);
    ~SDLRenderer();

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
    void invalidateTextCache(const Font *font);
    std::size_t textCacheSize() const { return textCache_.size(); }

private:
    SDL_Renderer *r_ = nullptr;
    Camera2D cam_{};
    int screenW_ = 800;
    int screenH_ = 600;

    float worldToScreenX(float worldX) const;
    float worldToScreenY(float worldY) const;

    struct TextKey
    {
        const Font *font = nullptr;
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;
        std::uint8_t a = 0;
        std::string text;
    };

    struct TextKeyHash
    {
        std::size_t operator()(const TextKey &k) const;
    };

    struct TextKeyEq
    {
        bool operator()(const TextKey &a, const TextKey &b) const;
    };

    struct TextCacheEntry
    {
        SDL_Texture *tex = nullptr;
        int w = 0;
        int h = 0;
        std::uint64_t lastUsed = 0;
    };

    void trimTextCache();

    std::unordered_map<TextKey, TextCacheEntry, TextKeyHash, TextKeyEq> textCache_;
    std::uint64_t textCacheCounter_ = 0;
    std::size_t textCacheLimit_ = 128;
};
