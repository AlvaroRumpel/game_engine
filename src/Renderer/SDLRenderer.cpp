#include "SDLRenderer.h"
#include "../Assets/Texture.h"
#include <SDL.h>
#include "../Assets/Font.h"
#include <SDL_ttf.h>
#include <cstdio>

SDLRenderer::SDLRenderer(SDL_Renderer *sdlRenderer) : r_(sdlRenderer) {}

SDLRenderer::~SDLRenderer()
{
    for (auto &kv : textCache_)
    {
        if (kv.second.tex)
            SDL_DestroyTexture(kv.second.tex);
    }
    textCache_.clear();
}

void SDLRenderer::beginFrame()
{
    SDL_SetRenderDrawColor(r_, 15, 15, 15, 255);
    SDL_RenderClear(r_);
}

void SDLRenderer::endFrame()
{
    SDL_RenderPresent(r_);
}

void SDLRenderer::drawRect(float x, float y, int w, int h,
                           unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    SDL_Rect rect;
    rect.x = (int)worldToScreenX(x);
    rect.y = (int)worldToScreenY(y);
    rect.w = (int)(w * cam_.zoom);
    rect.h = (int)(h * cam_.zoom);

    SDL_SetRenderDrawColor(r_, r, g, b, a);
    SDL_RenderFillRect(r_, &rect);
}

void SDLRenderer::drawTexture(const Texture &tex, float x, float y, float scale)
{
    if (!tex.native_)
        return;

    SDL_SetTextureBlendMode(tex.native_, SDL_BLENDMODE_BLEND);

    SDL_Rect dst;
    dst.x = (int)worldToScreenX(x);
    dst.y = (int)worldToScreenY(y);

    float s = scale * cam_.zoom;
    dst.w = (int)(tex.width_ * s);
    dst.h = (int)(tex.height_ * s);

    SDL_RenderCopy(r_, tex.native_, nullptr, &dst);
}

void SDLRenderer::drawText(const Font &font, const std::string &text,
                           float x, float y,
                           unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    if (!font.native_)
        return;
    if (text.empty())
        return;

    TextKey key;
    key.font = &font;
    key.r = r;
    key.g = g;
    key.b = b;
    key.a = a;
    key.text = text;

    auto it = textCache_.find(key);
    if (it == textCache_.end())
    {
        SDL_Color color{r, g, b, a};
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font.native_, text.c_str(), color);
        if (!surf)
        {
            std::printf("TTF_RenderUTF8_Blended failed: %s\n", TTF_GetError());
            return;
        }

        SDL_Texture *tex = SDL_CreateTextureFromSurface(r_, surf);
        if (!tex)
        {
            std::printf("SDL_CreateTextureFromSurface(text) failed: %s\n", SDL_GetError());
            SDL_FreeSurface(surf);
            return;
        }

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

        TextCacheEntry entry;
        entry.tex = tex;
        entry.w = surf->w;
        entry.h = surf->h;
        entry.lastUsed = ++textCacheCounter_;
        SDL_FreeSurface(surf);

        textCache_.insert({key, entry});
        trimTextCache();
        it = textCache_.find(key);
        if (it == textCache_.end())
            return;
    }
    else
    {
        it->second.lastUsed = ++textCacheCounter_;
    }

    SDL_Rect dst;
    dst.x = (int)x;
    dst.y = (int)y;
    dst.w = it->second.w;
    dst.h = it->second.h;

    int rc = SDL_RenderCopy(r_, it->second.tex, nullptr, &dst);
    if (rc != 0)
    {
        std::printf("SDL_RenderCopy(text) failed: %s\n", SDL_GetError());
    }
}

void SDLRenderer::setCamera(const Camera2D &cam, int screenW, int screenH)
{
    cam_ = cam;
    screenW_ = screenW;
    screenH_ = screenH;
}

void SDLRenderer::invalidateTextCache(const Font *font)
{
    if (!font)
        return;

    for (auto it = textCache_.begin(); it != textCache_.end();)
    {
        if (it->first.font == font)
        {
            if (it->second.tex)
                SDL_DestroyTexture(it->second.tex);
            it = textCache_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

float SDLRenderer::worldToScreenX(float worldX) const
{
    // câmera centrada: (world - camCenter) * zoom + halfScreen
    return (worldX - cam_.x) * cam_.zoom + (screenW_ * 0.5f);
}

float SDLRenderer::worldToScreenY(float worldY) const
{
    return (worldY - cam_.y) * cam_.zoom + (screenH_ * 0.5f);
}

std::size_t SDLRenderer::TextKeyHash::operator()(const TextKey &k) const
{
    std::size_t h = std::hash<const Font *>{}(k.font);
    h ^= std::hash<std::string>{}(k.text) + 0x9e3779b9 + (h << 6) + (h >> 2);
    std::size_t color = (std::size_t)k.r | ((std::size_t)k.g << 8) | ((std::size_t)k.b << 16) | ((std::size_t)k.a << 24);
    h ^= color + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

bool SDLRenderer::TextKeyEq::operator()(const TextKey &a, const TextKey &b) const
{
    return a.font == b.font && a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a && a.text == b.text;
}

void SDLRenderer::trimTextCache()
{
    while (textCache_.size() > textCacheLimit_)
    {
        auto lru = textCache_.end();
        for (auto it = textCache_.begin(); it != textCache_.end(); ++it)
        {
            if (lru == textCache_.end() || it->second.lastUsed < lru->second.lastUsed)
                lru = it;
        }
        if (lru == textCache_.end())
            break;
        if (lru->second.tex)
            SDL_DestroyTexture(lru->second.tex);
        textCache_.erase(lru);
    }
}
