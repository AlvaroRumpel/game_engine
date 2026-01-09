#include "AssetManifest.h"
#include <cstdio>
#include <fstream>
#include <sstream>

static bool IsCommentOrEmpty(const std::string &line)
{
    for (char c : line)
    {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
            continue;
        return (c == '#' || c == ';');
    }
    return true;
}

bool AssetManifest::loadFromFile(const std::string &path)
{
    textures_.clear();
    fonts_.clear();

    std::ifstream file(path);
    if (!file.is_open())
    {
        std::printf("AssetManifest: failed to open '%s'\n", path.c_str());
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line))
    {
        lineNumber++;
        if (IsCommentOrEmpty(line))
            continue;

        std::istringstream iss(line);
        std::string type;
        iss >> type;
        if (type == "texture")
        {
            std::string id;
            std::string filePath;
            iss >> id >> filePath;
            if (id.empty() || filePath.empty())
            {
                std::printf("AssetManifest: invalid texture line %d\n", lineNumber);
                continue;
            }
            textures_[id] = filePath;
        }
        else if (type == "font")
        {
            std::string id;
            std::string filePath;
            int size = 0;
            iss >> id >> filePath >> size;
            if (id.empty() || filePath.empty() || size <= 0)
            {
                std::printf("AssetManifest: invalid font line %d\n", lineNumber);
                continue;
            }
            FontDef def;
            def.path = filePath;
            def.size = size;
            fonts_[id] = def;
        }
        else
        {
            std::printf("AssetManifest: unknown entry '%s' on line %d\n", type.c_str(), lineNumber);
        }
    }

    return true;
}

const std::string *AssetManifest::texturePath(const std::string &id) const
{
    auto it = textures_.find(id);
    if (it == textures_.end())
        return nullptr;
    return &it->second;
}

const FontDef *AssetManifest::fontDef(const std::string &id) const
{
    auto it = fonts_.find(id);
    if (it == fonts_.end())
        return nullptr;
    return &it->second;
}
