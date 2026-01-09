#pragma once

#include <string>
#include <filesystem>
#include <SDL_ttf.h>

class Font
{
public:
    Font() = default;
    ~Font() = default;

    Font(const Font &) = delete;
    Font &operator=(const Font &) = delete;

    int size() const { return size_; }
    const std::string &path() const { return path_; }

private:
    friend class AssetManager;
    friend class SDLRenderer;

    TTF_Font *native_ = nullptr; // <-- agora é o tipo real da SDL_ttf
    int size_ = 0;
    std::string path_;

    std::filesystem::file_time_type lastWrite_{};
};
