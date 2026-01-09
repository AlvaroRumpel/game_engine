#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <filesystem>

class Texture;
class Font;
class SDLRenderer;
class AssetManifest;
struct FontDef;

class AssetManager
{
public:
    explicit AssetManager(SDLRenderer &renderer);
    ~AssetManager();

    bool loadManifest(const std::string &path);

    std::shared_ptr<Texture> loadTextureById(const std::string &id);
    std::shared_ptr<Texture> loadTexture(const std::string &path);

    // novo
    std::shared_ptr<Font> loadFontById(const std::string &id);
    std::shared_ptr<Font> loadFont(const std::string &path, int ptSize);

    void clear();

    void updateHotReload();
    bool manifestLoaded() const { return manifestLoaded_; }

private:
    SDLRenderer &renderer_;

    std::unique_ptr<AssetManifest> manifest_;
    std::string manifestPath_;
    std::filesystem::file_time_type manifestLastWrite_{};
    bool manifestLoaded_ = false;

    std::unordered_map<std::string, std::weak_ptr<Texture>> textures_;
    std::unordered_map<std::string, std::shared_ptr<Texture>> texturesById_;
    std::unordered_map<std::string, std::string> texturePathById_;

    // novo: chave = "path|size"
    std::unordered_map<std::string, std::weak_ptr<Font>> fonts_;
    std::unordered_map<std::string, std::shared_ptr<Font>> fontsById_;
    std::unordered_map<std::string, FontDef> fontDefById_;

    bool reloadTextureInPlace(Texture &tex);
    bool reloadTextureFromPath(Texture &tex, const std::string &newPath, const std::shared_ptr<Texture> &handle);
    bool reloadFontInPlace(Font &font);
    bool reloadFontFromDef(Font &font, const FontDef &def, const std::shared_ptr<Font> &handle);
};
