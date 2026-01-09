#pragma once
#include <string>
#include <unordered_map>

struct FontDef
{
    std::string path;
    int size = 0;
};

class AssetManifest
{
public:
    bool loadFromFile(const std::string &path);

    const std::string *texturePath(const std::string &id) const;
    const FontDef *fontDef(const std::string &id) const;

private:
    std::unordered_map<std::string, std::string> textures_;
    std::unordered_map<std::string, FontDef> fonts_;
};
