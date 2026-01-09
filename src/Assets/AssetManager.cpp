#include "AssetManager.h"
#include "Texture.h"
#include "Font.h"
#include "../Renderer/SDLRenderer.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <filesystem>
namespace fs = std::filesystem;

static fs::file_time_type SafeLastWrite(const std::string &path)
{
    std::error_code ec;
    auto t = fs::last_write_time(path, ec);
    if (ec)
        return fs::file_time_type{};
    return t;
}

static std::string FontKey(const std::string &path, int size)
{
    return path + "|" + std::to_string(size);
}

AssetManager::AssetManager(SDLRenderer &renderer) : renderer_(renderer)
{
    // SDL_image (PNG)
    int imgFlags = IMG_INIT_PNG;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags)
    {
        std::printf("IMG_Init error: %s\n", IMG_GetError());
    }

    // SDL_ttf
    if (TTF_Init() != 0)
    {
        std::printf("TTF_Init error: %s\n", TTF_GetError());
    }
}

AssetManager::~AssetManager()
{
    clear();
    TTF_Quit();
    IMG_Quit();
}

std::shared_ptr<Texture> AssetManager::loadTexture(const std::string &path)
{
    auto it = textures_.find(path);
    if (it != textures_.end())
    {
        if (auto existing = it->second.lock())
            return existing;
    }

    SDL_Surface *surf = IMG_Load(path.c_str());
    if (!surf)
    {
        std::printf("IMG_Load failed for '%s': %s\n", path.c_str(), IMG_GetError());
        return nullptr;
    }

    SDL_Texture *sdlTex = SDL_CreateTextureFromSurface(renderer_.native(), surf);
    if (!sdlTex)
    {
        std::printf("SDL_CreateTextureFromSurface failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return nullptr;
    }

    auto tex = std::make_shared<Texture>();
    tex->native_ = sdlTex;
    tex->width_ = surf->w;
    tex->height_ = surf->h;
    tex->path_ = path;
    tex->lastWrite_ = SafeLastWrite(path);

    SDL_FreeSurface(surf);

    textures_[path] = tex;
    return tex;
}

std::shared_ptr<Font> AssetManager::loadFont(const std::string &path, int ptSize)
{
    const std::string key = FontKey(path, ptSize);

    auto it = fonts_.find(key);
    if (it != fonts_.end())
    {
        if (auto existing = it->second.lock())
            return existing;
    }

    TTF_Font *f = TTF_OpenFont(path.c_str(), ptSize);
    if (!f)
    {
        std::printf("TTF_OpenFont failed for '%s' size=%d: %s\n", path.c_str(), ptSize, TTF_GetError());
        return nullptr;
    }

    auto font = std::make_shared<Font>();
    font->native_ = f;
    font->size_ = ptSize;
    font->path_ = path;
    font->lastWrite_ = SafeLastWrite(path);

    fonts_[key] = font;
    return font;
}

void AssetManager::clear()
{
    // destruir texturas
    for (auto &kv : textures_)
    {
        if (auto t = kv.second.lock())
        {
            if (t->native_)
            {
                SDL_DestroyTexture(t->native_);
                t->native_ = nullptr;
            }
        }
    }
    textures_.clear();

    // destruir fontes
    for (auto &kv : fonts_)
    {
        if (auto f = kv.second.lock())
        {
            if (f->native_)
            {
                TTF_CloseFont(f->native_);
                f->native_ = nullptr;
            }
        }
    }
    fonts_.clear();
}

bool AssetManager::reloadTextureInPlace(Texture &tex)
{
    SDL_Surface *surf = IMG_Load(tex.path_.c_str());
    if (!surf)
    {
        std::printf("HotReload IMG_Load failed '%s': %s\n", tex.path_.c_str(), IMG_GetError());
        return false;
    }

    SDL_Texture *newTex = SDL_CreateTextureFromSurface(renderer_.native(), surf);
    if (!newTex)
    {
        std::printf("HotReload CreateTexture failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return false;
    }

    SDL_FreeSurface(surf);

    if (tex.native_)
        SDL_DestroyTexture(tex.native_);
    tex.native_ = newTex;

    int w = 0, h = 0;
    SDL_QueryTexture(tex.native_, nullptr, nullptr, &w, &h);
    tex.width_ = w;
    tex.height_ = h;

    tex.lastWrite_ = SafeLastWrite(tex.path_);
    std::printf("HotReload texture OK: %s\n", tex.path_.c_str());
    return true;
}

bool AssetManager::reloadFontInPlace(Font &font)
{
    TTF_Font *newFont = TTF_OpenFont(font.path_.c_str(), font.size_);
    if (!newFont)
    {
        std::printf("HotReload OpenFont failed '%s': %s\n", font.path_.c_str(), TTF_GetError());
        return false;
    }

    if (font.native_)
        TTF_CloseFont(font.native_);
    font.native_ = newFont;

    font.lastWrite_ = SafeLastWrite(font.path_);
    std::printf("HotReload font OK: %s\n", font.path_.c_str());
    return true;
}

static bool IsStable(const std::string &path, std::filesystem::file_time_type t)
{
    using namespace std::chrono;
    // considera estável se já passou um pouco desde a última escrita
    // file_time_type -> converte de forma simples via now (heurística ok no Windows)
    auto now = std::filesystem::file_time_type::clock::now();
    return (now - t) > 200ms;
}
void AssetManager::updateHotReload()
{
    for (auto &kv : textures_)
    {
        if (auto t = kv.second.lock())
        {
            auto now = SafeLastWrite(t->path_);
            if (now != fs::file_time_type{} && now != t->lastWrite_ && IsStable(t->path_, now))
            {
                reloadTextureInPlace(*t);
            }
        }
    }

    for (auto &kv : fonts_)
    {
        if (auto f = kv.second.lock())
        {
            auto now = SafeLastWrite(f->path_);
            if (now != fs::file_time_type{} && now != f->lastWrite_)
            {
                reloadFontInPlace(*f);
            }
        }
    }
}
