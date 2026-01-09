#define SDL_MAIN_HANDLED
#include "Engine/Engine.h"
#include "Game/SandboxScenes.h"
#include "Assets/Texture.h"
#include "World/SceneManager.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>
#include <string>
#include <vector>

#include <imgui.h>
#include "ThirdParty/imgui/backends/imgui_impl_sdl2.h"
#include "ThirdParty/imgui/backends/imgui_impl_sdlrenderer2.h"

static std::string ToLower(const std::string &s);
static bool IsTextureExt(const std::string &ext);
static void ClampEntityToBounds(const Scene::Bounds &b, Entity &e);

static void DrawFileTree(const std::filesystem::path &root, std::string &selectedPath, bool allowDrag)
{
    if (!std::filesystem::exists(root))
        return;

    std::vector<std::filesystem::directory_entry> entries;
    for (auto &entry : std::filesystem::directory_iterator(root))
        entries.push_back(entry);

    std::sort(entries.begin(), entries.end(),
              [](const auto &a, const auto &b)
              {
                  if (a.is_directory() != b.is_directory())
                      return a.is_directory();
                  return a.path().filename().string() < b.path().filename().string();
              });

    for (auto &entry : entries)
    {
        const auto &path = entry.path();
        std::string name = path.filename().string();
        if (entry.is_directory())
        {
            if (ImGui::TreeNode(name.c_str()))
            {
                DrawFileTree(path, selectedPath, allowDrag);
                ImGui::TreePop();
            }
        }
        else
        {
            bool selected = (selectedPath == path.string());
            if (ImGui::Selectable(name.c_str(), selected))
                selectedPath = path.string();
            if (allowDrag)
            {
                std::string ext = ToLower(path.extension().string());
                if (IsTextureExt(ext))
                {
                    if (ImGui::BeginDragDropSource())
                    {
                        std::string p = path.string();
                        ImGui::SetDragDropPayload("ASSET_PATH", p.c_str(), p.size() + 1);
                        ImGui::Text("Sprite: %s", name.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
            }
        }
    }
}

static void OpenInVSCode(const std::string &path)
{
    if (path.empty())
        return;
    std::string cmd = "code \"" + path + "\"";
    std::system(cmd.c_str());
}

static std::string ToLower(const std::string &s)
{
    std::string out = s;
    for (char &c : out)
    {
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    }
    return out;
}

static bool IsTextureExt(const std::string &ext)
{
    return ext == ".png" || ext == ".bmp" || ext == ".jpg" || ext == ".jpeg";
}

static bool IsFontExt(const std::string &ext)
{
    return ext == ".ttf" || ext == ".otf";
}

static std::string MakeUniquePath(const std::filesystem::path &dst)
{
    if (!std::filesystem::exists(dst))
        return dst.string();

    std::filesystem::path base = dst.stem();
    std::filesystem::path ext = dst.extension();
    std::filesystem::path dir = dst.parent_path();

    for (int i = 1; i < 1000; ++i)
    {
        std::filesystem::path candidate = dir / (base.string() + "_" + std::to_string(i) + ext.string());
        if (!std::filesystem::exists(candidate))
            return candidate.string();
    }
    return dst.string();
}

static std::string MakeUniqueId(const std::string &baseId, const std::vector<std::string> &used)
{
    auto exists = [&](const std::string &id)
    {
        for (const auto &u : used)
        {
            if (u == id)
                return true;
        }
        return false;
    };

    if (!exists(baseId))
        return baseId;

    for (int i = 1; i < 1000; ++i)
    {
        std::string candidate = baseId + "_" + std::to_string(i);
        if (!exists(candidate))
            return candidate;
    }
    return baseId;
}

static bool UpdateManifest(const std::string &manifestPath, const std::string &type,
                           const std::string &filePath, int fontSize,
                           std::string &outId)
{
    std::vector<std::string> lines;
    std::vector<std::string> usedIds;
    std::ifstream file(manifestPath);
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
        {
            lines.push_back(line);
            std::istringstream iss(line);
            std::string t;
            std::string id;
            iss >> t >> id;
            if (!id.empty())
                usedIds.push_back(id);
        }
    }

    std::filesystem::path p(filePath);
    std::string baseId = p.stem().string();
    outId = MakeUniqueId(baseId, usedIds);

    std::string entry;
    if (type == "texture")
    {
        entry = "texture " + outId + " " + filePath;
    }
    else if (type == "font")
    {
        entry = "font " + outId + " " + filePath + " " + std::to_string(fontSize);
    }
    else
    {
        return false;
    }

    lines.push_back(entry);
    std::ofstream out(manifestPath, std::ios::trunc);
    if (!out.is_open())
        return false;
    for (const auto &l : lines)
        out << l << "\n";
    return true;
}

static bool ImportAsset(const std::string &srcPath, const std::string &dstDir,
                        bool overwrite, bool autoUnique,
                        std::string &outStatus, std::string &outNewPath, std::string &outId,
                        bool &outConflict)
{
    std::filesystem::path src(srcPath);
    if (!std::filesystem::exists(src))
    {
        outStatus = "Source file not found.";
        outConflict = false;
        return false;
    }

    std::string ext = ToLower(src.extension().string());
    std::string type;
    int fontSize = 20;
    if (IsTextureExt(ext))
        type = "texture";
    else if (IsFontExt(ext))
        type = "font";
    else
    {
        outStatus = "Unsupported asset type.";
        outConflict = false;
        return false;
    }

    std::filesystem::path dstRoot = std::filesystem::path(dstDir.empty() ? "assets" : dstDir);
    std::filesystem::create_directories(dstRoot);
    std::filesystem::path dst = dstRoot / src.filename();
    std::string dstPath = dst.string();

    bool exists = std::filesystem::exists(dst);
    outConflict = exists;
    if (exists && !overwrite)
    {
        if (autoUnique)
        {
            dstPath = MakeUniquePath(dst);
            outConflict = false;
        }
        else
        {
            outStatus = "File exists. Choose overwrite or make copy.";
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::copy_file(src, dstPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        outStatus = "Copy failed: " + ec.message();
        outConflict = false;
        return false;
    }

    outNewPath = dstPath;
    std::string manifestPath = "assets/manifest.txt";
    if (!UpdateManifest(manifestPath, type, dstPath, fontSize, outId))
    {
        outStatus = "Manifest update failed.";
        outConflict = false;
        return false;
    }

    outStatus = "Imported " + type + " as '" + outId + "'";
    outConflict = false;
    return true;
}

static void ScreenToWorld(const Camera2D &cam, float sx, float sy, int viewW, int viewH, float &wx, float &wy)
{
    wx = (sx - (viewW * 0.5f)) / cam.zoom + cam.x;
    wy = (sy - (viewH * 0.5f)) / cam.zoom + cam.y;
}

static void WorldToScreen(const Camera2D &cam, float wx, float wy, int viewW, int viewH, float &sx, float &sy)
{
    sx = (wx - cam.x) * cam.zoom + (viewW * 0.5f);
    sy = (wy - cam.y) * cam.zoom + (viewH * 0.5f);
}

static bool PointInRect(float px, float py, float x, float y, float w, float h)
{
    return px >= x && py >= y && px <= (x + w) && py <= (y + h);
}

enum class BoundsType
{
    None,
    Rect,
    Collider,
    Sprite
};

static bool GetEntityBounds(const Entity &e, float &x, float &y, float &w, float &h, BoundsType &type)
{
    if (e.collider.enabled)
    {
        x = e.transform.x + e.collider.offsetX;
        y = e.transform.y + e.collider.offsetY;
        w = e.collider.w;
        h = e.collider.h;
        type = BoundsType::Collider;
        return true;
    }
    if (e.rect.enabled)
    {
        x = e.transform.x;
        y = e.transform.y;
        w = (float)e.rect.w;
        h = (float)e.rect.h;
        type = BoundsType::Rect;
        return true;
    }
    if (e.sprite.enabled && e.sprite.texture)
    {
        x = e.transform.x;
        y = e.transform.y;
        w = (float)e.sprite.texture->width() * e.sprite.scale;
        h = (float)e.sprite.texture->height() * e.sprite.scale;
        type = BoundsType::Sprite;
        return true;
    }
    type = BoundsType::None;
    return false;
}

static bool PickEntityAt(Scene &scene, float wx, float wy, int &outId)
{
    float bestArea = 0.0f;
    int bestId = 0;

    for (const auto &e : scene.entities())
    {
        bool hit = false;
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;

        if (e.collider.enabled)
        {
            x = e.transform.x + e.collider.offsetX;
            y = e.transform.y + e.collider.offsetY;
            w = e.collider.w;
            h = e.collider.h;
            hit = PointInRect(wx, wy, x, y, w, h);
        }
        else if (e.rect.enabled)
        {
            x = e.transform.x;
            y = e.transform.y;
            w = (float)e.rect.w;
            h = (float)e.rect.h;
            hit = PointInRect(wx, wy, x, y, w, h);
        }

        if (hit)
        {
            float area = w * h;
            if (bestId == 0 || area < bestArea)
            {
                bestArea = area;
                bestId = e.id;
            }
        }
    }

    if (bestId != 0)
    {
        outId = bestId;
        return true;
    }
    return false;
}

static float SnapValue(float v, float step)
{
    if (step <= 0.0f)
        return v;
    float inv = 1.0f / step;
    return std::round(v * inv) / inv;
}

static bool SaveScene(const Scene &scene, const Camera2D &cam, const std::string &path)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return false;

    out << "{\n";
    out << "  \"camera\": {\"x\": " << cam.x << ", \"y\": " << cam.y << ", \"zoom\": " << cam.zoom << "},\n";
    out << "  \"bounds\": {\"enabled\": " << (scene.bounds().enabled ? "true" : "false")
        << ", \"x\": " << scene.bounds().x << ", \"y\": " << scene.bounds().y
        << ", \"w\": " << scene.bounds().w << ", \"h\": " << scene.bounds().h << "},\n";
    out << "  \"entities\": [\n";

    const auto &entities = scene.entities();
    for (size_t i = 0; i < entities.size(); ++i)
    {
        const auto &e = entities[i];
        out << "    {\n";
        out << "      \"id\": " << e.id << ",\n";
        out << "      \"x\": " << e.transform.x << ", \"y\": " << e.transform.y << ",\n";
        out << "      \"rect\": {\"enabled\": " << (e.rect.enabled ? "true" : "false")
            << ", \"w\": " << e.rect.w << ", \"h\": " << e.rect.h
            << ", \"r\": " << (int)e.rect.r << ", \"g\": " << (int)e.rect.g
            << ", \"b\": " << (int)e.rect.b << ", \"a\": " << (int)e.rect.a << "},\n";
        out << "      \"sprite\": {\"enabled\": " << (e.sprite.enabled ? "true" : "false")
            << ", \"scale\": " << e.sprite.scale << ", \"rot\": " << e.sprite.rotationDeg;
        if (e.sprite.texture)
            out << ", \"path\": \"" << e.sprite.texture->path() << "\"";
        out << "},\n";
        out << "      \"collider\": {\"enabled\": " << (e.collider.enabled ? "true" : "false")
            << ", \"w\": " << e.collider.w << ", \"h\": " << e.collider.h
            << ", \"offX\": " << e.collider.offsetX << ", \"offY\": " << e.collider.offsetY
            << ", \"trigger\": " << (e.collider.isTrigger ? "true" : "false") << "},\n";
        out << "      \"rigidbody\": {\"enabled\": " << (e.rigidbody.enabled ? "true" : "false")
            << ", \"vx\": " << e.rigidbody.vx << ", \"vy\": " << e.rigidbody.vy
            << ", \"kin\": " << (e.rigidbody.isKinematic ? "true" : "false") << "}\n";
        out << "    }";
        if (i + 1 < entities.size())
            out << ",";
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
    return true;
}

static bool ExtractValue(const std::string &line, const std::string &key, std::string &out)
{
    size_t pos = line.find(key);
    if (pos == std::string::npos)
        return false;
    pos = line.find(':', pos);
    if (pos == std::string::npos)
        return false;
    pos += 1;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        pos++;
    size_t end = pos;
    while (end < line.size() && line[end] != ',' && line[end] != '}' && line[end] != '\r' && line[end] != '\n')
        end++;
    out = line.substr(pos, end - pos);
    return true;
}

static bool ParseBool(const std::string &s)
{
    return s.find("true") != std::string::npos;
}

static float ParseFloat(const std::string &s)
{
    return std::strtof(s.c_str(), nullptr);
}

static int ParseInt(const std::string &s)
{
    return std::atoi(s.c_str());
}

struct EntitySnapshot
{
    float x = 0.0f;
    float y = 0.0f;

    bool rectEnabled = false;
    int rectW = 0;
    int rectH = 0;
    int rectR = 255, rectG = 255, rectB = 255, rectA = 255;

    bool spriteEnabled = false;
    float spriteScale = 1.0f;
    float spriteRot = 0.0f;
    std::string spritePath;

    bool colliderEnabled = false;
    float colW = 0.0f;
    float colH = 0.0f;
    float colOffX = 0.0f;
    float colOffY = 0.0f;
    bool colTrigger = false;

    bool rbEnabled = false;
    float rbVx = 0.0f;
    float rbVy = 0.0f;
    bool rbKin = false;
};

static bool LoadScene(Scene &scene, AssetManager &assets, Camera2D &cam, const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::vector<EntitySnapshot> list;
    EntitySnapshot current{};
    bool inEntity = false;
    enum class Section
    {
        None,
        Entity,
        Rect,
        Sprite,
        Collider,
        RigidBody
    } section = Section::None;

    std::string line;
    while (std::getline(file, line))
    {
        std::string v;
        if (line.find("\"camera\"") != std::string::npos)
        {
            section = Section::None;
        }
        if (line.find("\"camera\"") != std::string::npos)
        {
            if (ExtractValue(line, "\"x\"", v))
                cam.x = ParseFloat(v);
            if (ExtractValue(line, "\"y\"", v))
                cam.y = ParseFloat(v);
            if (ExtractValue(line, "\"zoom\"", v))
                cam.zoom = ParseFloat(v);
        }
        if (line.find("\"bounds\"") != std::string::npos)
        {
            if (ExtractValue(line, "\"enabled\"", v))
                scene.bounds().enabled = ParseBool(v);
            if (ExtractValue(line, "\"x\"", v))
                scene.bounds().x = ParseFloat(v);
            if (ExtractValue(line, "\"y\"", v))
                scene.bounds().y = ParseFloat(v);
            if (ExtractValue(line, "\"w\"", v))
                scene.bounds().w = ParseFloat(v);
            if (ExtractValue(line, "\"h\"", v))
                scene.bounds().h = ParseFloat(v);
        }

        if (line.find("{") != std::string::npos && line.find("\"id\"") != std::string::npos)
        {
            current = EntitySnapshot{};
            section = Section::Entity;
            inEntity = true;
        }
        if (!inEntity)
            continue;

        if (line.find("\"rect\"") != std::string::npos)
            section = Section::Rect;
        else if (line.find("\"sprite\"") != std::string::npos)
            section = Section::Sprite;
        else if (line.find("\"collider\"") != std::string::npos)
            section = Section::Collider;
        else if (line.find("\"rigidbody\"") != std::string::npos)
            section = Section::RigidBody;

        if (section == Section::Entity)
        {
            if (ExtractValue(line, "\"x\"", v))
                current.x = ParseFloat(v);
            if (ExtractValue(line, "\"y\"", v))
                current.y = ParseFloat(v);
        }
        else if (section == Section::Rect)
        {
            if (ExtractValue(line, "\"enabled\"", v))
                current.rectEnabled = ParseBool(v);
            if (ExtractValue(line, "\"w\"", v))
                current.rectW = ParseInt(v);
            if (ExtractValue(line, "\"h\"", v))
                current.rectH = ParseInt(v);
            if (ExtractValue(line, "\"r\"", v))
                current.rectR = ParseInt(v);
            if (ExtractValue(line, "\"g\"", v))
                current.rectG = ParseInt(v);
            if (ExtractValue(line, "\"b\"", v))
                current.rectB = ParseInt(v);
            if (ExtractValue(line, "\"a\"", v))
                current.rectA = ParseInt(v);
        }
        else if (section == Section::Sprite)
        {
            if (ExtractValue(line, "\"enabled\"", v))
                current.spriteEnabled = ParseBool(v);
            if (ExtractValue(line, "\"scale\"", v))
                current.spriteScale = ParseFloat(v);
            if (ExtractValue(line, "\"rot\"", v))
                current.spriteRot = ParseFloat(v);
            if (ExtractValue(line, "\"path\"", v))
            {
                if (!v.empty() && v.front() == '\"')
                    v.erase(0, 1);
                if (!v.empty() && v.back() == '\"')
                    v.pop_back();
                current.spritePath = v;
            }
        }
        else if (section == Section::Collider)
        {
            if (ExtractValue(line, "\"enabled\"", v))
                current.colliderEnabled = ParseBool(v);
            if (ExtractValue(line, "\"w\"", v))
                current.colW = ParseFloat(v);
            if (ExtractValue(line, "\"h\"", v))
                current.colH = ParseFloat(v);
            if (ExtractValue(line, "\"offX\"", v))
                current.colOffX = ParseFloat(v);
            if (ExtractValue(line, "\"offY\"", v))
                current.colOffY = ParseFloat(v);
            if (ExtractValue(line, "\"trigger\"", v))
                current.colTrigger = ParseBool(v);
        }
        else if (section == Section::RigidBody)
        {
            if (ExtractValue(line, "\"enabled\"", v))
                current.rbEnabled = ParseBool(v);
            if (ExtractValue(line, "\"vx\"", v))
                current.rbVx = ParseFloat(v);
            if (ExtractValue(line, "\"vy\"", v))
                current.rbVy = ParseFloat(v);
            if (ExtractValue(line, "\"kin\"", v))
                current.rbKin = ParseBool(v);
        }

        if (line.find("}") != std::string::npos && inEntity && section == Section::RigidBody)
        {
            list.push_back(current);
            inEntity = false;
            section = Section::None;
        }
    }

    scene.clearEntities();
    for (const auto &snap : list)
    {
        Entity &e = scene.createEntity();
        e.transform.x = snap.x;
        e.transform.y = snap.y;

        e.rect.enabled = snap.rectEnabled;
        e.rect.w = snap.rectW;
        e.rect.h = snap.rectH;
        e.rect.r = (unsigned char)snap.rectR;
        e.rect.g = (unsigned char)snap.rectG;
        e.rect.b = (unsigned char)snap.rectB;
        e.rect.a = (unsigned char)snap.rectA;

        e.sprite.enabled = snap.spriteEnabled;
        e.sprite.scale = snap.spriteScale;
        e.sprite.rotationDeg = snap.spriteRot;
        if (!snap.spritePath.empty())
            e.sprite.texture = assets.loadTexture(snap.spritePath);
        else
            e.sprite.texture.reset();

        e.collider.enabled = snap.colliderEnabled;
        e.collider.w = snap.colW;
        e.collider.h = snap.colH;
        e.collider.offsetX = snap.colOffX;
        e.collider.offsetY = snap.colOffY;
        e.collider.isTrigger = snap.colTrigger;

        e.rigidbody.enabled = snap.rbEnabled;
        e.rigidbody.vx = snap.rbVx;
        e.rigidbody.vy = snap.rbVy;
        e.rigidbody.isKinematic = snap.rbKin;
    }

    return true;
}
static void ApplyEditorStyle()
{
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(10, 8);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.34f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.34f, 0.40f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.36f, 0.42f, 0.50f, 1.00f);
}

struct ProjectInfo
{
    std::string name;
    std::string root;
    std::string scenePath;
};

static bool WriteProjectFile(const ProjectInfo &proj)
{
    std::filesystem::path p = std::filesystem::path(proj.root) / "project.json";
    std::ofstream out(p.string(), std::ios::trunc);
    if (!out.is_open())
        return false;
    out << "{\n";
    out << "  \"name\": \"" << proj.name << "\",\n";
    out << "  \"scene\": \"" << proj.scenePath << "\"\n";
    out << "}\n";
    return true;
}

static bool ReadProjectFile(const std::string &root, ProjectInfo &proj)
{
    std::filesystem::path p = std::filesystem::path(root) / "project.json";
    std::ifstream in(p.string());
    if (!in.is_open())
        return false;
    proj.root = root;
    proj.name.clear();
    proj.scenePath.clear();
    std::string line;
    while (std::getline(in, line))
    {
        std::string v;
        if (ExtractValue(line, "\"name\"", v))
        {
            if (!v.empty() && v.front() == '\"')
                v.erase(0, 1);
            if (!v.empty() && v.back() == '\"')
                v.pop_back();
            proj.name = v;
        }
        if (ExtractValue(line, "\"scene\"", v))
        {
            if (!v.empty() && v.front() == '\"')
                v.erase(0, 1);
            if (!v.empty() && v.back() == '\"')
                v.pop_back();
            proj.scenePath = v;
        }
    }
    return true;
}

static bool CreateProjectFolders(const std::string &root)
{
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(root) / "assets", ec);
    std::filesystem::create_directories(std::filesystem::path(root) / "src" / "Scripts", ec);
    std::filesystem::create_directories(std::filesystem::path(root) / "build", ec);
    return !ec;
}

static bool BuildProjectRelease(const std::string &root)
{
    std::string buildDir = (std::filesystem::path(root) / "build").string();
    std::string cmd1 = "cmake -S \"" + root + "\" -B \"" + buildDir + "\"";
    std::string cmd2 = "cmake --build \"" + buildDir + "\" --config Release";
    int r1 = std::system(cmd1.c_str());
    int r2 = std::system(cmd2.c_str());
    return (r1 == 0 && r2 == 0);
}

static bool WriteTextFile(const std::string &path, const std::string &content)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open())
        return false;
    out << content;
    return true;
}

static bool CreateScriptSkeleton(const std::string &root, const std::string &name, bool openVSCode)
{
    if (name.empty())
        return false;
    std::filesystem::path dir = std::filesystem::path(root) / "src" / "Scripts";
    std::filesystem::create_directories(dir);
    std::filesystem::path hPath = dir / (name + ".h");
    std::filesystem::path cppPath = dir / (name + ".cpp");

    std::string header =
        "#pragma once\n"
        "class Engine;\n"
        "class " + name + "\n"
        "{\n"
        "public:\n"
        "    void onUpdate(Engine &engine, float dt);\n"
        "};\n";

    std::string source =
        "#include \"" + name + ".h\"\n"
        "#include \"../../Engine/Engine.h\"\n\n"
        "void " + name + "::onUpdate(Engine &engine, float dt)\n"
        "{\n"
        "    (void)engine;\n"
        "    (void)dt;\n"
        "}\n";

    bool ok = WriteTextFile(hPath.string(), header) && WriteTextFile(cppPath.string(), source);
    if (ok && openVSCode)
    {
        OpenInVSCode(hPath.string());
        OpenInVSCode(cppPath.string());
    }
    return ok;
}

static void CreatePrefabScripts(const std::string &root)
{
    CreateScriptSkeleton(root, "PlayerMovement", false);
    CreateScriptSkeleton(root, "CameraFollow", false);
}

static void CreateEntityFromSprite(Scene &scene, AssetManager &assets, const std::string &path, float wx, float wy)
{
    auto tex = assets.loadTexture(path);
    if (!tex)
        return;
    Entity &e = scene.createEntity();
    e.transform.x = wx;
    e.transform.y = wy;
    e.sprite.texture = tex;
    e.sprite.enabled = true;
    e.sprite.scale = 1.0f;
    e.rect.enabled = false;
    e.collider.enabled = true;
    e.collider.w = (float)tex->width();
    e.collider.h = (float)tex->height();
    ClampEntityToBounds(scene.bounds(), e);
}

static void ClampEntityToBounds(const Scene::Bounds &b, Entity &e)
{
    if (!b.enabled)
        return;
    if (e.transform.x < b.x)
        e.transform.x = b.x;
    if (e.transform.y < b.y)
        e.transform.y = b.y;
    if (e.transform.x > (b.x + b.w))
        e.transform.x = b.x + b.w;
    if (e.transform.y > (b.y + b.h))
        e.transform.y = b.y + b.h;
}

int main()
{
    Engine engine;
    if (!engine.start())
        return 1;

    engine.setScene(CreateDemoScene());
    engine.tick(0.0f);

    SDL_Window *window = engine.nativeWindow();
    SDL_Renderer *renderer = engine.nativeSDLRenderer();
    SDL_SetWindowResizable(window, SDL_TRUE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    ApplyEditorStyle();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    enum class PlayState
    {
        Stopped,
        Playing,
        Paused
    };
    PlayState playState = PlayState::Stopped;
    std::string selectedPath;
    int selectedEntityId = 0;
    std::string importStatus = "Drop files into the window to import.";
    std::vector<std::string> pendingDrops;
    char importSubdirBuf[128] = {};
    bool importOverwrite = false;
    bool importAutoUnique = true;
    std::string pendingConflictPath;
    std::string pendingConflictDstDir;
    bool conflictOpen = false;
    SDL_Texture *sceneTexture = nullptr;
    int sceneTexW = 0;
    int sceneTexH = 0;
    bool gameViewInputActive = false;
    ImVec2 lastGameViewMin(0.0f, 0.0f);
    ImVec2 lastGameViewMax(0.0f, 0.0f);
    bool draggingMove = false;
    bool draggingScale = false;
    bool draggingRotate = false;
    int dragEntityId = 0;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;
    BoundsType dragType = BoundsType::None;
    float dragStartRot = 0.0f;
    float dragStartAngle = 0.0f;
    bool snapEnabled = true;
    float snapMove = 16.0f;
    float snapScale = 8.0f;
    float snapRotate = 15.0f;
    bool camFollowEntity = false;
    bool camLockPos = false;
    int camFollowId = 0;
    float camLockX = 0.0f;
    float camLockY = 0.0f;
    bool camClampToBounds = false;
    bool savedOk = false;
    char savePathBuf[256] = "assets/editor_scene.json";
    bool dockInitialized = false;
    std::vector<EntitySnapshot> playSnapshot;
    Camera2D playSnapshotCam{};
    SceneManager sceneManager;
    ProjectInfo project;
    char projectRootBuf[256] = ".";
    char projectNameBuf[128] = "MyProject";
    std::string projectStatus;
    char scriptNameBuf[64] = "NewScript";
    bool openScriptAfterCreate = true;

    Uint32 lastTicks = SDL_GetTicks();

    while (true)
    {
        engine.beginInputFrame();

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_DROPFILE)
            {
                char *dropped = e.drop.file;
                if (dropped)
                {
                    pendingDrops.push_back(dropped);
                    SDL_free(dropped);
                }
                continue;
            }
            if (e.type == SDL_QUIT)
                engine.handleEvent(e);
            else
            {
                ImGuiIO &io = ImGui::GetIO();
                bool allowGameInput = (playState == PlayState::Playing) && gameViewInputActive;
                if (!io.WantCaptureKeyboard && !io.WantCaptureMouse)
                    engine.handleEvent(e);
                else if (allowGameInput)
                    engine.handleEvent(e);
            }
        }

        engine.finalizeInput();
        if (engine.shouldQuit())
            break;

        Uint32 nowTicks = SDL_GetTicks();
        float rawDt = (nowTicks - lastTicks) / 1000.0f;
        lastTicks = nowTicks;

        engine.tick((playState == PlayState::Playing) ? rawDt : 0.0f);

        Scene &scene = engine.scene();

        if (playState != PlayState::Playing)
        {
            Camera2D &cam = engine.camera();
            float camSpeed = 400.0f;
            if (gameViewInputActive)
            {
                float ix = engine.input().getAxis("MoveX");
                float iy = engine.input().getAxis("MoveY");
                cam.x += ix * camSpeed * rawDt;
                cam.y += iy * camSpeed * rawDt;
            }

            if (camFollowEntity)
            {
                Entity *e = engine.scene().findEntity(camFollowId);
                if (e)
                {
                    cam.x = e->transform.x;
                    cam.y = e->transform.y;
                }
            }
            else if (camLockPos)
            {
                cam.x = camLockX;
                cam.y = camLockY;
            }
            if (camClampToBounds && scene.bounds().enabled)
            {
                if (cam.x < scene.bounds().x)
                    cam.x = scene.bounds().x;
                if (cam.y < scene.bounds().y)
                    cam.y = scene.bounds().y;
                if (cam.x > scene.bounds().x + scene.bounds().w)
                    cam.x = scene.bounds().x + scene.bounds().w;
                if (cam.y > scene.bounds().y + scene.bounds().h)
                    cam.y = scene.bounds().y + scene.bounds().h;
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

#ifdef IMGUI_HAS_DOCK
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                     ImGuiWindowFlags_NoNavFocus;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("DockSpace", nullptr, dockFlags);
        ImGui::PopStyleVar(2);

        ImGuiID dockId = ImGui::GetID("EditorDockspace");
        ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        if (!dockInitialized)
        {
            dockInitialized = true;
            ImGui::DockBuilderRemoveNode(dockId);
            ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockId, viewport->Size);

            ImGuiID dockMain = dockId;
            ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.08f, nullptr, &dockMain);
            ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.20f, nullptr, &dockMain);
            ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.22f, nullptr, &dockMain);
            ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);

            ImGui::DockBuilderDockWindow("Toolbar", dockTop);
            ImGui::DockBuilderDockWindow("Project", dockLeft);
            ImGui::DockBuilderDockWindow("Inspector", dockRight);
            ImGui::DockBuilderDockWindow("Console", dockBottom);
            ImGui::DockBuilderDockWindow("Game View", dockMain);
            ImGui::DockBuilderDockWindow("Camera", dockRight);

            ImGui::DockBuilderFinish(dockId);
        }

        ImGui::End();
#endif

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImVec2 vpPos = viewport->Pos;
        ImVec2 vpSize = viewport->Size;
        ImGui::SetNextWindowPos(ImVec2(vpPos.x, vpPos.y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(vpSize.x, 60.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::InputText("Save Path", savePathBuf, sizeof(savePathBuf));
        if (ImGui::Button(playState == PlayState::Playing ? "Stop" : "Play"))
        {
            if (playState == PlayState::Playing || playState == PlayState::Paused)
            {
                playState = PlayState::Stopped;
                scene.clearEntities();
                for (const auto &snap : playSnapshot)
                {
                    Entity &e = scene.createEntity();
                    e.transform.x = snap.x;
                    e.transform.y = snap.y;
                    e.rect.enabled = snap.rectEnabled;
                    e.rect.w = snap.rectW;
                    e.rect.h = snap.rectH;
                    e.rect.r = (unsigned char)snap.rectR;
                    e.rect.g = (unsigned char)snap.rectG;
                    e.rect.b = (unsigned char)snap.rectB;
                    e.rect.a = (unsigned char)snap.rectA;
                    e.sprite.enabled = snap.spriteEnabled;
                    e.sprite.scale = snap.spriteScale;
                    e.sprite.rotationDeg = snap.spriteRot;
                    if (!snap.spritePath.empty())
                        e.sprite.texture = engine.assets().loadTexture(snap.spritePath);
                    else
                        e.sprite.texture.reset();
                    e.collider.enabled = snap.colliderEnabled;
                    e.collider.w = snap.colW;
                    e.collider.h = snap.colH;
                    e.collider.offsetX = snap.colOffX;
                    e.collider.offsetY = snap.colOffY;
                    e.collider.isTrigger = snap.colTrigger;
                    e.rigidbody.enabled = snap.rbEnabled;
                    e.rigidbody.vx = snap.rbVx;
                    e.rigidbody.vy = snap.rbVy;
                    e.rigidbody.isKinematic = snap.rbKin;
                }
                engine.camera() = playSnapshotCam;
            }
            else
            {
                playSnapshot.clear();
                for (const auto &e : scene.entities())
                {
                    EntitySnapshot snap{};
                    snap.x = e.transform.x;
                    snap.y = e.transform.y;
                    snap.rectEnabled = e.rect.enabled;
                    snap.rectW = e.rect.w;
                    snap.rectH = e.rect.h;
                    snap.rectR = e.rect.r;
                    snap.rectG = e.rect.g;
                    snap.rectB = e.rect.b;
                    snap.rectA = e.rect.a;
                    snap.spriteEnabled = e.sprite.enabled;
                    snap.spriteScale = e.sprite.scale;
                    snap.spriteRot = e.sprite.rotationDeg;
                    if (e.sprite.texture)
                        snap.spritePath = e.sprite.texture->path();
                    snap.colliderEnabled = e.collider.enabled;
                    snap.colW = e.collider.w;
                    snap.colH = e.collider.h;
                    snap.colOffX = e.collider.offsetX;
                    snap.colOffY = e.collider.offsetY;
                    snap.colTrigger = e.collider.isTrigger;
                    snap.rbEnabled = e.rigidbody.enabled;
                    snap.rbVx = e.rigidbody.vx;
                    snap.rbVy = e.rigidbody.vy;
                    snap.rbKin = e.rigidbody.isKinematic;
                    playSnapshot.push_back(snap);
                }
                playSnapshotCam = engine.camera();
                playState = PlayState::Playing;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(playState == PlayState::Paused ? "Resume" : "Pause"))
        {
            if (playState == PlayState::Playing)
                playState = PlayState::Paused;
            else if (playState == PlayState::Paused)
                playState = PlayState::Playing;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            engine.setScene(CreateDemoScene());
            engine.tick(0.0f);
            camFollowEntity = false;
            camLockPos = false;
            playState = PlayState::Stopped;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            savedOk = SaveScene(engine.scene(), engine.camera(), savePathBuf);
            importStatus = savedOk ? "Scene saved." : "Failed to save scene.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            bool ok = LoadScene(scene, engine.assets(), engine.camera(), savePathBuf);
            importStatus = ok ? "Scene loaded." : "Failed to load scene.";
            playState = PlayState::Stopped;
        }
        ImGui::Text("FPS: %.1f", engine.time().fps());
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(vpPos.x, vpPos.y + 70.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, vpSize.y - 220.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Project", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::Text("Project");
        ImGui::InputText("Root", projectRootBuf, sizeof(projectRootBuf));
        ImGui::InputText("Name", projectNameBuf, sizeof(projectNameBuf));
        if (ImGui::Button("Create"))
        {
            project.root = projectRootBuf;
            project.name = projectNameBuf;
            project.scenePath = "assets/editor_scene.json";
            if (CreateProjectFolders(project.root) && WriteProjectFile(project))
            {
                sceneManager.setProjectRoot(project.root);
                sceneManager.setCurrentScenePath(project.scenePath);
                std::filesystem::path fullScene = std::filesystem::path(project.root) / project.scenePath;
                std::snprintf(savePathBuf, sizeof(savePathBuf), "%s", fullScene.string().c_str());
                projectStatus = "Project created.";
            }
            else
            {
                projectStatus = "Failed to create project.";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Open"))
        {
            ProjectInfo loaded;
            if (ReadProjectFile(projectRootBuf, loaded))
            {
                project = loaded;
                std::snprintf(projectNameBuf, sizeof(projectNameBuf), "%s", project.name.c_str());
                sceneManager.setProjectRoot(project.root);
                sceneManager.setCurrentScenePath(project.scenePath);
                std::filesystem::path fullScene = std::filesystem::path(project.root) / project.scenePath;
                std::snprintf(savePathBuf, sizeof(savePathBuf), "%s", fullScene.string().c_str());
                bool ok = LoadScene(scene, engine.assets(), engine.camera(), savePathBuf);
                projectStatus = ok ? "Project opened." : "Opened project (scene load failed).";
            }
            else
            {
                projectStatus = "Project not found.";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Project"))
        {
            project.root = projectRootBuf;
            project.name = projectNameBuf;
            if (project.scenePath.empty())
                project.scenePath = "assets/editor_scene.json";
            bool ok = SaveScene(scene, engine.camera(), savePathBuf) && WriteProjectFile(project);
            projectStatus = ok ? "Project saved." : "Failed to save project.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Build Release"))
        {
            projectStatus = BuildProjectRelease(projectRootBuf) ? "Build OK." : "Build failed.";
        }
        ImGui::TextWrapped("%s", projectStatus.c_str());
        ImGui::Separator();
        ImGui::Text("Scripts");
        ImGui::InputText("Script Name", scriptNameBuf, sizeof(scriptNameBuf));
        ImGui::Checkbox("Open in VSCode", &openScriptAfterCreate);
        if (ImGui::Button("Create Script"))
        {
            std::string root = projectRootBuf;
            bool ok = CreateScriptSkeleton(root, scriptNameBuf, openScriptAfterCreate);
            projectStatus = ok ? "Script created." : "Failed to create script.";
        }
        ImGui::SameLine();
        if (ImGui::Button("Create Prefabs"))
        {
            CreatePrefabScripts(projectRootBuf);
            projectStatus = "Prefab scripts created.";
        }
        ImGui::Separator();
        ImGui::Text("assets/");
        DrawFileTree("assets", selectedPath, true);
        ImGui::Separator();
        ImGui::Text("src/");
        DrawFileTree("src", selectedPath, false);
        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedPath.empty() ? "-" : selectedPath.c_str());
        if (ImGui::Button("Open in VSCode") && !selectedPath.empty())
            OpenInVSCode(selectedPath);
        ImGui::Separator();
        ImGui::Text("Import Settings");
        ImGui::InputText("Subfolder (assets/)", importSubdirBuf, sizeof(importSubdirBuf));
        ImGui::Checkbox("Overwrite if exists", &importOverwrite);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-unique", &importAutoUnique);
        ImGui::Separator();
        ImGui::Text("Drop Zone");
        ImGui::BeginChild("AssetsDropZone", ImVec2(0, 60), true);
        std::string subdirPreview = importSubdirBuf;
        ImGui::TextWrapped("Drop files here to import into assets/%s", subdirPreview.empty() ? "" : subdirPreview.c_str());
        ImGui::EndChild();
        bool dropZoneHovered = ImGui::IsItemHovered();

        if (!pendingDrops.empty() && dropZoneHovered)
        {
            std::string subdir = importSubdirBuf;
            std::string dstDir = "assets";
            if (!subdir.empty())
                dstDir = (std::filesystem::path("assets") / subdir).string();

            for (const auto &drop : pendingDrops)
            {
                std::string newPath;
                std::string newId;
                bool conflict = false;
                if (ImportAsset(drop, dstDir, importOverwrite, importAutoUnique, importStatus, newPath, newId, conflict))
                {
                    selectedPath = newPath;
                }
                else if (conflict)
                {
                    pendingConflictPath = drop;
                    pendingConflictDstDir = dstDir;
                    conflictOpen = true;
                }
            }
            pendingDrops.clear();
        }

        if (conflictOpen)
            ImGui::OpenPopup("Import Conflict");

        if (ImGui::BeginPopupModal("Import Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("File already exists in destination.");
            ImGui::Separator();
            if (ImGui::Button("Overwrite"))
            {
                std::string newPath;
                std::string newId;
                bool conflict = false;
                ImportAsset(pendingConflictPath, pendingConflictDstDir, true, false, importStatus, newPath, newId, conflict);
                selectedPath = newPath;
                conflictOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Make Copy"))
            {
                std::string newPath;
                std::string newId;
                bool conflict = false;
                ImportAsset(pendingConflictPath, pendingConflictDstDir, false, true, importStatus, newPath, newId, conflict);
                selectedPath = newPath;
                conflictOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                importStatus = "Import cancelled.";
                conflictOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(vpPos.x + vpSize.x - 320.0f, vpPos.y + 70.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, vpSize.y - 220.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::Text("Entities");
        if (ImGui::Button("Add Entity"))
        {
            Entity &e = engine.scene().createEntity();
            e.transform.x = engine.camera().x;
            e.transform.y = engine.camera().y;
            e.rect.enabled = true;
            e.rect.w = 32;
            e.rect.h = 32;
            selectedEntityId = e.id;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete") && selectedEntityId != 0)
        {
            engine.scene().destroyEntity(selectedEntityId);
            selectedEntityId = 0;
        }
        for (const auto &e : engine.scene().entities())
        {
            std::string label = "Entity " + std::to_string(e.id);
            bool selected = (selectedEntityId == e.id);
            if (ImGui::Selectable(label.c_str(), selected))
                selectedEntityId = e.id;
        }
        ImGui::Separator();
        Entity *selected = engine.scene().findEntity(selectedEntityId);
        if (selected)
        {
            ImGui::Text("Transform");
            ImGui::InputFloat("X", &selected->transform.x);
            ImGui::InputFloat("Y", &selected->transform.y);

            ImGui::Separator();
            ImGui::Text("RectRender");
            ImGui::Checkbox("Rect Enabled", &selected->rect.enabled);
            ImGui::InputInt("Rect W", &selected->rect.w);
            ImGui::InputInt("Rect H", &selected->rect.h);

            ImGui::Separator();
            ImGui::Text("SpriteRender");
            ImGui::Checkbox("Sprite Enabled", &selected->sprite.enabled);
            ImGui::InputFloat("Sprite Scale", &selected->sprite.scale);
            ImGui::InputFloat("Sprite Rotation", &selected->sprite.rotationDeg);

            ImGui::Separator();
            ImGui::Text("Collider");
            ImGui::Checkbox("Collider Enabled", &selected->collider.enabled);
            ImGui::InputFloat("Collider W", &selected->collider.w);
            ImGui::InputFloat("Collider H", &selected->collider.h);
            ImGui::Checkbox("Is Trigger", &selected->collider.isTrigger);

            ImGui::Separator();
            ImGui::Text("RigidBody2D");
            ImGui::Checkbox("RB Enabled", &selected->rigidbody.enabled);
            ImGui::Checkbox("Kinematic", &selected->rigidbody.isKinematic);
            ImGui::InputFloat("Vel X", &selected->rigidbody.vx);
            ImGui::InputFloat("Vel Y", &selected->rigidbody.vy);
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(vpPos.x + 330.0f, vpPos.y + 70.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(vpSize.x - 660.0f, vpSize.y - 240.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Game View", nullptr, ImGuiWindowFlags_NoCollapse);
        const char *simLabel = "Stopped";
        if (playState == PlayState::Playing)
            simLabel = "Running";
        else if (playState == PlayState::Paused)
            simLabel = "Paused";
        ImGui::Text("Simulation: %s", simLabel);
        Camera2D &cam = engine.camera();
        if (playState != PlayState::Playing)
        {
            ImGui::SameLine();
            if (ImGui::Button("Center Cam"))
            {
                cam.x = 0.0f;
                cam.y = 0.0f;
            }
        }
        ImVec2 avail = ImGui::GetContentRegionAvail();
        int desiredW = (int)avail.x;
        int desiredH = (int)avail.y;
        if (desiredW < 1)
            desiredW = 1;
        if (desiredH < 1)
            desiredH = 1;

        if (!sceneTexture || desiredW != sceneTexW || desiredH != sceneTexH)
        {
            if (sceneTexture)
            {
                SDL_DestroyTexture(sceneTexture);
                sceneTexture = nullptr;
            }
            sceneTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, desiredW, desiredH);
            if (sceneTexture)
            {
                SDL_SetTextureBlendMode(sceneTexture, SDL_BLENDMODE_BLEND);
                sceneTexW = desiredW;
                sceneTexH = desiredH;
            }
        }

        bool gameViewHovered = false;
        bool gameViewFocused = false;
        float mouseWorldX = 0.0f;
        float mouseWorldY = 0.0f;
        bool haveMouseWorld = false;

        if (sceneTexture)
        {
            SDL_SetRenderTarget(renderer, sceneTexture);
            engine.renderWorld(false, sceneTexW, sceneTexH);
            SDL_SetRenderTarget(renderer, nullptr);
            ImGui::Image((ImTextureID)sceneTexture, ImVec2((float)sceneTexW, (float)sceneTexH));
            ImVec2 imgMin = ImGui::GetItemRectMin();
            ImVec2 imgMax = ImGui::GetItemRectMax();
            lastGameViewMin = imgMin;
            lastGameViewMax = imgMax;
            gameViewHovered = ImGui::IsItemHovered();
            gameViewFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            if (gameViewHovered)
            {
            ImVec2 mousePos = ImGui::GetMousePos();
            float localX = mousePos.x - imgMin.x;
            float localY = mousePos.y - imgMin.y;
            if (localX >= 0.0f && localY >= 0.0f && localX <= (float)sceneTexW && localY <= (float)sceneTexH)
            {
                ScreenToWorld(cam, localX, localY, sceneTexW, sceneTexH, mouseWorldX, mouseWorldY);
                haveMouseWorld = true;
                if (!draggingMove && !draggingScale && !draggingRotate && ImGui::IsMouseClicked(0))
                {
                    int pickedId = 0;
                    if (PickEntityAt(engine.scene(), mouseWorldX, mouseWorldY, pickedId))
                        selectedEntityId = pickedId;
                }
                if (playState != PlayState::Playing && ImGui::IsMouseDragging(1))
                {
                    ImVec2 delta = ImGui::GetIO().MouseDelta;
                    cam.x -= delta.x / cam.zoom;
                    cam.y -= delta.y / cam.zoom;
                }
                if (playState != PlayState::Playing && ImGui::GetIO().MouseWheel != 0.0f)
                {
                    float zoomFactor = (ImGui::GetIO().MouseWheel > 0.0f) ? 1.1f : 0.9f;
                    cam.zoom *= zoomFactor;
                    if (cam.zoom < 0.1f)
                        cam.zoom = 0.1f;
                }
            }
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
            {
                const char *path = (const char *)payload->Data;
                if (path && haveMouseWorld)
                {
                    CreateEntityFromSprite(scene, engine.assets(), path, mouseWorldX, mouseWorldY);
                }
            }
            ImGui::EndDragDropTarget();
        }
        }

        ImGui::End();

        ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::Text("View");
        ImGui::InputFloat("Cam X", &cam.x);
        ImGui::InputFloat("Cam Y", &cam.y);
        ImGui::InputFloat("Zoom", &cam.zoom);
        if (cam.zoom < 0.1f)
            cam.zoom = 0.1f;
        if (haveMouseWorld)
            ImGui::Text("Mouse World: %.1f, %.1f", mouseWorldX, mouseWorldY);
        ImGui::Separator();
        ImGui::Text("Gizmo");
        ImGui::Checkbox("Snap", &snapEnabled);
        ImGui::SameLine();
        ImGui::InputFloat("Move Step", &snapMove);
        ImGui::SameLine();
        ImGui::InputFloat("Scale Step", &snapScale);
        ImGui::SameLine();
        ImGui::InputFloat("Rotate Step", &snapRotate);
        ImGui::Separator();
        ImGui::Text("Camera Lock");
        if (ImGui::Button("Follow Selected") && selectedEntityId != 0)
        {
            camFollowEntity = true;
            camLockPos = false;
            camFollowId = selectedEntityId;
        }
        ImGui::SameLine();
        if (ImGui::Button("Lock Position"))
        {
            camLockPos = true;
            camFollowEntity = false;
            camLockX = cam.x;
            camLockY = cam.y;
        }
        ImGui::SameLine();
        if (ImGui::Button("Unlock"))
        {
            camFollowEntity = false;
            camLockPos = false;
        }
        ImGui::End();

        ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::Text("Bounds");
        ImGui::Checkbox("Enable Bounds", &scene.bounds().enabled);
        ImGui::InputFloat("Bound X", &scene.bounds().x);
        ImGui::InputFloat("Bound Y", &scene.bounds().y);
        ImGui::InputFloat("Bound W", &scene.bounds().w);
        ImGui::InputFloat("Bound H", &scene.bounds().h);
        ImGui::Checkbox("Clamp Camera", &camClampToBounds);
        ImGui::End();

        if (sceneTexture)
        {
            ImDrawList *dl = ImGui::GetForegroundDrawList();
            if (scene.bounds().enabled)
            {
                float bx0 = 0.0f, by0 = 0.0f, bx1 = 0.0f, by1 = 0.0f;
                WorldToScreen(cam, scene.bounds().x, scene.bounds().y, sceneTexW, sceneTexH, bx0, by0);
                WorldToScreen(cam, scene.bounds().x + scene.bounds().w, scene.bounds().y + scene.bounds().h, sceneTexW, sceneTexH, bx1, by1);
                ImVec2 b0(lastGameViewMin.x + bx0, lastGameViewMin.y + by0);
                ImVec2 b1(lastGameViewMin.x + bx1, lastGameViewMin.y + by1);
                dl->AddRect(b0, b1, IM_COL32(120, 180, 255, 180), 0.0f, 0, 2.0f);
            }

            if (selectedEntityId != 0)
            {
                Entity *selectedEnt = engine.scene().findEntity(selectedEntityId);
                float bx = 0.0f, by = 0.0f, bw = 0.0f, bh = 0.0f;
                BoundsType btype = BoundsType::None;
                if (selectedEnt && GetEntityBounds(*selectedEnt, bx, by, bw, bh, btype))
                {
                    float sx0 = 0.0f, sy0 = 0.0f, sx1 = 0.0f, sy1 = 0.0f;
                    WorldToScreen(cam, bx, by, sceneTexW, sceneTexH, sx0, sy0);
                    WorldToScreen(cam, bx + bw, by + bh, sceneTexW, sceneTexH, sx1, sy1);

                    ImVec2 p0(lastGameViewMin.x + sx0, lastGameViewMin.y + sy0);
                    ImVec2 p1(lastGameViewMin.x + sx1, lastGameViewMin.y + sy1);

                    ImU32 outline = IM_COL32(255, 200, 80, 220);
                    dl->AddRect(p0, p1, outline, 0.0f, 0, 2.0f);

                    ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
                    ImVec2 handleSize(8.0f, 8.0f);
                    ImVec2 moveMin(center.x - handleSize.x, center.y - handleSize.y);
                    ImVec2 moveMax(center.x + handleSize.x, center.y + handleSize.y);
                    dl->AddRectFilled(moveMin, moveMax, IM_COL32(80, 200, 255, 220));

                    ImVec2 scaleMin(p1.x - handleSize.x, p1.y - handleSize.y);
                    ImVec2 scaleMax(p1.x + handleSize.x, p1.y + handleSize.y);
                    dl->AddRectFilled(scaleMin, scaleMax, IM_COL32(200, 200, 80, 220));

                    ImVec2 rotCenter((p0.x + p1.x) * 0.5f, p0.y - 18.0f);
                    float rotRadius = 6.0f;
                    dl->AddCircleFilled(rotCenter, rotRadius, IM_COL32(200, 120, 240, 220));

                    ImVec2 mousePos = ImGui::GetMousePos();
                    bool overMove = mousePos.x >= moveMin.x && mousePos.x <= moveMax.x && mousePos.y >= moveMin.y && mousePos.y <= moveMax.y;
                    bool overScale = mousePos.x >= scaleMin.x && mousePos.x <= scaleMax.x && mousePos.y >= scaleMin.y && mousePos.y <= scaleMax.y;
                    bool overRotate = (std::abs(mousePos.x - rotCenter.x) <= rotRadius && std::abs(mousePos.y - rotCenter.y) <= rotRadius);

                    if (gameViewHovered && ImGui::IsMouseClicked(0))
                    {
                        if (overMove)
                        {
                            draggingMove = true;
                            draggingScale = false;
                            draggingRotate = false;
                            dragEntityId = selectedEntityId;
                            dragOffsetX = mouseWorldX - selectedEnt->transform.x;
                            dragOffsetY = mouseWorldY - selectedEnt->transform.y;
                        }
                        else if (overScale)
                        {
                            draggingScale = true;
                            draggingMove = false;
                            draggingRotate = false;
                            dragEntityId = selectedEntityId;
                            dragType = btype;
                        }
                        else if (overRotate && btype == BoundsType::Sprite)
                        {
                            draggingRotate = true;
                            draggingMove = false;
                            draggingScale = false;
                            dragEntityId = selectedEntityId;
                            dragStartRot = selectedEnt->sprite.rotationDeg;
                            dragStartAngle = std::atan2(mouseWorldY - (by + bh * 0.5f), mouseWorldX - (bx + bw * 0.5f));
                        }
                    }

                    if (draggingMove && dragEntityId == selectedEntityId && haveMouseWorld)
                    {
                        float newX = mouseWorldX - dragOffsetX;
                        float newY = mouseWorldY - dragOffsetY;
                        bool lockX = ImGui::GetIO().KeyShift;
                        bool lockY = ImGui::GetIO().KeyCtrl;
                        if (lockX)
                            newY = selectedEnt->transform.y;
                        if (lockY)
                            newX = selectedEnt->transform.x;
                        if (snapEnabled)
                        {
                            newX = SnapValue(newX, snapMove);
                            newY = SnapValue(newY, snapMove);
                        }
                        selectedEnt->transform.x = newX;
                        selectedEnt->transform.y = newY;
                        ClampEntityToBounds(scene.bounds(), *selectedEnt);
                        if (ImGui::IsMouseReleased(0))
                            draggingMove = false;
                    }
                    else if (draggingScale && dragEntityId == selectedEntityId && haveMouseWorld)
                    {
                        float newW = mouseWorldX - bx;
                        float newH = mouseWorldY - by;
                        if (newW < 1.0f)
                            newW = 1.0f;
                        if (newH < 1.0f)
                            newH = 1.0f;

                        if (snapEnabled)
                        {
                            newW = SnapValue(newW, snapScale);
                            newH = SnapValue(newH, snapScale);
                        }

                        if (dragType == BoundsType::Rect)
                        {
                            selectedEnt->rect.w = (int)newW;
                            selectedEnt->rect.h = (int)newH;
                        }
                        else if (dragType == BoundsType::Collider)
                        {
                            selectedEnt->collider.w = newW;
                            selectedEnt->collider.h = newH;
                        }
                        else if (dragType == BoundsType::Sprite && selectedEnt->sprite.texture)
                        {
                            float baseW = (float)selectedEnt->sprite.texture->width();
                            float baseH = (float)selectedEnt->sprite.texture->height();
                            float scaleW = newW / baseW;
                            float scaleH = newH / baseH;
                            float newScale = (scaleW > scaleH) ? scaleW : scaleH;
                            if (newScale < 0.05f)
                                newScale = 0.05f;
                            selectedEnt->sprite.scale = newScale;
                        }

                        ClampEntityToBounds(scene.bounds(), *selectedEnt);
                        if (ImGui::IsMouseReleased(0))
                            draggingScale = false;
                    }
                    else if (draggingRotate && dragEntityId == selectedEntityId && haveMouseWorld)
                    {
                        float angle = std::atan2(mouseWorldY - (by + bh * 0.5f), mouseWorldX - (bx + bw * 0.5f));
                        float delta = angle - dragStartAngle;
                        float deg = dragStartRot + (delta * 180.0f / 3.14159265f);
                        if (snapEnabled)
                            deg = SnapValue(deg, snapRotate);
                        selectedEnt->sprite.rotationDeg = deg;
                        ClampEntityToBounds(scene.bounds(), *selectedEnt);
                        if (ImGui::IsMouseReleased(0))
                            draggingRotate = false;
                    }
                }
            }
        }

        ImGui::SetNextWindowPos(ImVec2(vpPos.x, vpPos.y + vpSize.y - 140.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(vpSize.x, 140.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped("%s", importStatus.c_str());
        ImGui::End();

        gameViewInputActive = (gameViewHovered || gameViewFocused) && !(draggingMove || draggingScale || draggingRotate);

        SDL_SetRenderTarget(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 18, 18, 20, 255);
        SDL_RenderClear(renderer);

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

        engine.present();
    }

    if (sceneTexture)
        SDL_DestroyTexture(sceneTexture);

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    engine.stop();
    return 0;
}
