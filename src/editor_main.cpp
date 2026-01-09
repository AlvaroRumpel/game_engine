#define SDL_MAIN_HANDLED
#include "Engine/Engine.h"
#include "Game/SandboxScenes.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <imgui.h>
#include "ThirdParty/imgui/backends/imgui_impl_sdl2.h"
#include "ThirdParty/imgui/backends/imgui_impl_sdlrenderer2.h"

static void DrawFileTree(const std::filesystem::path &root, std::string &selectedPath)
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
                DrawFileTree(path, selectedPath);
                ImGui::TreePop();
            }
        }
        else
        {
            bool selected = (selectedPath == path.string());
            if (ImGui::Selectable(name.c_str(), selected))
                selectedPath = path.string();
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

static bool ImportAsset(const std::string &srcPath, std::string &outStatus, std::string &outNewPath, std::string &outId)
{
    std::filesystem::path src(srcPath);
    if (!std::filesystem::exists(src))
    {
        outStatus = "Source file not found.";
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
        return false;
    }

    std::filesystem::path dstDir = std::filesystem::path("assets");
    std::filesystem::create_directories(dstDir);
    std::filesystem::path dst = dstDir / src.filename();
    std::string dstPath = MakeUniquePath(dst);

    std::error_code ec;
    std::filesystem::copy_file(src, dstPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        outStatus = "Copy failed: " + ec.message();
        return false;
    }

    outNewPath = dstPath;
    std::string manifestPath = "assets/manifest.txt";
    if (!UpdateManifest(manifestPath, type, dstPath, fontSize, outId))
    {
        outStatus = "Manifest update failed.";
        return false;
    }

    outStatus = "Imported " + type + " as '" + outId + "'";
    return true;
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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    bool simulate = false;
    std::string selectedPath;
    int selectedEntityId = 0;
    std::string importStatus = "Drop files into the window to import.";

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
                    std::string newPath;
                    std::string newId;
                    if (ImportAsset(dropped, importStatus, newPath, newId))
                        selectedPath = newPath;
                    SDL_free(dropped);
                }
                continue;
            }
            if (e.type == SDL_QUIT)
                engine.handleEvent(e);
            else
            {
                ImGuiIO &io = ImGui::GetIO();
                if (!io.WantCaptureKeyboard && !io.WantCaptureMouse)
                    engine.handleEvent(e);
            }
        }

        engine.finalizeInput();
        if (engine.shouldQuit())
            break;

        Uint32 nowTicks = SDL_GetTicks();
        float rawDt = (nowTicks - lastTicks) / 1000.0f;
        lastTicks = nowTicks;

        engine.tick(simulate ? rawDt : 0.0f);
        engine.renderWorld(false);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Toolbar");
        if (ImGui::Button(simulate ? "Stop" : "Play"))
        {
            if (simulate)
            {
                simulate = false;
                engine.setScene(CreateDemoScene());
                engine.tick(0.0f);
            }
            else
            {
                simulate = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            engine.setScene(CreateDemoScene());
            engine.tick(0.0f);
        }
        ImGui::Text("FPS: %.1f", engine.time().fps());
        ImGui::End();

        ImGui::Begin("Project");
        ImGui::Text("assets/");
        DrawFileTree("assets", selectedPath);
        ImGui::Separator();
        ImGui::Text("src/");
        DrawFileTree("src", selectedPath);
        ImGui::Separator();
        ImGui::Text("Selected: %s", selectedPath.empty() ? "-" : selectedPath.c_str());
        if (ImGui::Button("Open in VSCode") && !selectedPath.empty())
            OpenInVSCode(selectedPath);
        ImGui::Separator();
        ImGui::TextWrapped("%s", importStatus.c_str());
        ImGui::End();

        ImGui::Begin("Inspector");
        ImGui::Text("Entities");
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

        ImGui::Begin("Scene");
        Camera2D &cam = engine.camera();
        ImGui::InputFloat("Cam X", &cam.x);
        ImGui::InputFloat("Cam Y", &cam.y);
        ImGui::InputFloat("Zoom", &cam.zoom);
        if (cam.zoom < 0.1f)
            cam.zoom = 0.1f;
        ImGui::End();

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

        engine.present();
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    engine.stop();
    return 0;
}
