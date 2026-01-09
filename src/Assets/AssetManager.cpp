#include "AssetManager.h"
#include "AssetManifest.h"
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

bool AssetManager::loadManifest(const std::string &path)
{
    if (!manifest_)
        manifest_ = std::make_unique<AssetManifest>();

    manifestPath_ = path;
    manifestLoaded_ = manifest_->loadFromFile(path);
    manifestLastWrite_ = SafeLastWrite(path);
    return manifestLoaded_;
}

std::shared_ptr<Texture> AssetManager::loadTextureById(const std::string &id)
{
    auto it = texturesById_.find(id);
    if (it != texturesById_.end())
        return it->second;

    if (!manifest_)
    {
        std::printf("AssetManager: manifest not loaded (texture id '%s')\n", id.c_str());
        return nullptr;
    }

    const std::string *path = manifest_->texturePath(id);
    if (!path)
    {
        std::printf("AssetManager: texture id '%s' not found in manifest\n", id.c_str());
        return nullptr;
    }

    auto tex = loadTexture(*path);
    if (tex)
    {
        texturesById_[id] = tex;
        texturePathById_[id] = *path;
    }
    return tex;
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

std::shared_ptr<Font> AssetManager::loadFontById(const std::string &id)
{
    auto it = fontsById_.find(id);
    if (it != fontsById_.end())
        return it->second;

    if (!manifest_)
    {
        std::printf("AssetManager: manifest not loaded (font id '%s')\n", id.c_str());
        return nullptr;
    }

    const FontDef *def = manifest_->fontDef(id);
    if (!def)
    {
        std::printf("AssetManager: font id '%s' not found in manifest\n", id.c_str());
        return nullptr;
    }

    auto font = loadFont(def->path, def->size);
    if (font)
    {
        fontsById_[id] = font;
        fontDefById_[id] = *def;
    }
    return font;
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
    texturesById_.clear();
    texturePathById_.clear();

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
    fontsById_.clear();
    fontDefById_.clear();
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

bool AssetManager::reloadTextureFromPath(Texture &tex, const std::string &newPath, const std::shared_ptr<Texture> &handle)
{
    SDL_Surface *surf = IMG_Load(newPath.c_str());
    if (!surf)
    {
        std::printf("HotReload IMG_Load failed '%s': %s\n", newPath.c_str(), IMG_GetError());
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

    std::string oldPath = tex.path_;
    tex.path_ = newPath;
    tex.lastWrite_ = SafeLastWrite(newPath);

    if (oldPath != newPath)
    {
        textures_.erase(oldPath);
        textures_[newPath] = handle;
    }

    std::printf("HotReload texture path OK: %s\n", newPath.c_str());
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
    renderer_.invalidateTextCache(&font);
    std::printf("HotReload font OK: %s\n", font.path_.c_str());
    return true;
}

bool AssetManager::reloadFontFromDef(Font &font, const FontDef &def, const std::shared_ptr<Font> &handle)
{
    TTF_Font *newFont = TTF_OpenFont(def.path.c_str(), def.size);
    if (!newFont)
    {
        std::printf("HotReload OpenFont failed '%s': %s\n", def.path.c_str(), TTF_GetError());
        return false;
    }

    if (font.native_)
        TTF_CloseFont(font.native_);
    font.native_ = newFont;

    std::string oldPath = font.path_;
    int oldSize = font.size_;
    font.path_ = def.path;
    font.size_ = def.size;
    font.lastWrite_ = SafeLastWrite(def.path);
    renderer_.invalidateTextCache(&font);

    std::string oldKey = FontKey(oldPath, oldSize);
    std::string newKey = FontKey(font.path_, font.size_);
    if (oldKey != newKey)
    {
        fonts_.erase(oldKey);
        fonts_[newKey] = handle;
    }

    std::printf("HotReload font path OK: %s\n", font.path_.c_str());
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
    if (manifest_ && !manifestPath_.empty())
    {
        auto now = SafeLastWrite(manifestPath_);
        if (now != std::filesystem::file_time_type{} && now != manifestLastWrite_ && IsStable(manifestPath_, now))
        {
            if (manifest_->loadFromFile(manifestPath_))
            {
                manifestLoaded_ = true;
                manifestLastWrite_ = now;

                for (auto &kv : texturesById_)
                {
                    const std::string *path = manifest_->texturePath(kv.first);
                    if (!path)
                        continue;
                    auto itPath = texturePathById_.find(kv.first);
                    if (itPath == texturePathById_.end() || itPath->second != *path)
                    {
                        if (kv.second)
                            reloadTextureFromPath(*kv.second, *path, kv.second);
                        texturePathById_[kv.first] = *path;
                    }
                }

                for (auto &kv : fontsById_)
                {
                    const FontDef *def = manifest_->fontDef(kv.first);
                    if (!def)
                        continue;
                    auto itDef = fontDefById_.find(kv.first);
                    if (itDef == fontDefById_.end() ||
                        itDef->second.path != def->path ||
                        itDef->second.size != def->size)
                    {
                        if (kv.second)
                            reloadFontFromDef(*kv.second, *def, kv.second);
                        fontDefById_[kv.first] = *def;
                    }
                }
            }
        }
    }

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
            if (now != fs::file_time_type{} && now != f->lastWrite_ && IsStable(f->path_, now))
            {
                reloadFontInPlace(*f);
            }
        }
    }
}
