#include "SDLRenderer.h"
#include "../Assets/Texture.h"
#include <SDL.h>
#include "../Assets/Font.h"
#include <SDL_ttf.h>
#include <cstdio>

SDLRenderer::SDLRenderer(SDL_Renderer *sdlRenderer) : r_(sdlRenderer) {}

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

    SDL_Rect dst;
    dst.x = (int)x;
    dst.y = (int)y;
    dst.w = surf->w;
    dst.h = surf->h;

    SDL_FreeSurface(surf);

    int rc = SDL_RenderCopy(r_, tex, nullptr, &dst);
    if (rc != 0)
    {
        std::printf("SDL_RenderCopy(text) failed: %s\n", SDL_GetError());
    }

    SDL_DestroyTexture(tex);
}

void SDLRenderer::setCamera(const Camera2D &cam, int screenW, int screenH)
{
    cam_ = cam;
    screenW_ = screenW;
    screenH_ = screenH;
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