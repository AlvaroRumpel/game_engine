#pragma once
#include <memory>
#include <string>
#include <unordered_map>

class Texture;
class Font;
class SDLRenderer;

class AssetManager
{
public:
    explicit AssetManager(SDLRenderer &renderer);
    ~AssetManager();

    std::shared_ptr<Texture> loadTexture(const std::string &path);

    // novo
    std::shared_ptr<Font> loadFont(const std::string &path, int ptSize);

    void clear();

    void updateHotReload();

private:
    SDLRenderer &renderer_;

    std::unordered_map<std::string, std::weak_ptr<Texture>> textures_;

    // novo: chave = "path|size"
    std::unordered_map<std::string, std::weak_ptr<Font>> fonts_;

    bool reloadTextureInPlace(Texture &tex);
    bool reloadFontInPlace(Font &font);
};
