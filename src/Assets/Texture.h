#pragma once
#include <string>
#include <filesystem>

struct SDL_Texture;

class Texture
{
public:
    Texture() = default;
    ~Texture() = default;

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;

    Texture(Texture &&) = default;
    Texture &operator=(Texture &&) = default;

    int width() const { return width_; }
    int height() const { return height_; }
    const std::string &path() const { return path_; }

private:
    friend class AssetManager;
    friend class SDLRenderer;

    SDL_Texture *native_ = nullptr; // backend SDL (por enquanto)
    int width_ = 0;
    int height_ = 0;
    std::string path_;

    std::filesystem::file_time_type lastWrite_{};
};
