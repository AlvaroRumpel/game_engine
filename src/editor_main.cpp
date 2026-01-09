#define SDL_MAIN_HANDLED
#include "Engine/Engine.h"
#include "Game/SandboxScenes.h"
#include "Assets/Texture.h"
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

static void DrawSplitter(bool vertical, float thickness, float &value, float minVal, float maxVal, float direction)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##splitter", size);
    bool active = ImGui::IsItemActive();
    if (active)
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (vertical)
            value += delta.x * direction;
        else
            value += delta.y * direction;
        if (value < minVal)
            value = minVal;
        if (value > maxVal)
            value = maxVal;
    }

    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 col = ImGui::GetColorU32(ImGuiCol_Separator);
    if (vertical)
        dl->AddLine(pos, ImVec2(pos.x, pos.y + size.y), col, 1.0f);
    else
        dl->AddLine(pos, ImVec2(pos.x + size.x, pos.y), col, 1.0f);
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
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    bool simulate = false;
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
    int dragEntityId = 0;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;
    BoundsType dragType = BoundsType::None;
    float leftPanelW = 280.0f;
    float rightPanelW = 280.0f;
    float consoleH = 120.0f;
    float toolbarH = 40.0f;

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
                bool allowGameInput = simulate && gameViewInputActive;
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

        engine.tick(simulate ? rawDt : 0.0f);

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        int winW = 800;
        int winH = 600;
        SDL_GetWindowSize(window, &winW, &winH);

        const float minPanelW = 200.0f;
        const float minConsoleH = 80.0f;
        const float minGameW = 300.0f;
        const float minGameH = 200.0f;

        float maxPanelW = (float)winW * 0.45f;
        if (leftPanelW < minPanelW)
            leftPanelW = minPanelW;
        if (rightPanelW < minPanelW)
            rightPanelW = minPanelW;
        if (leftPanelW > maxPanelW)
            leftPanelW = maxPanelW;
        if (rightPanelW > maxPanelW)
            rightPanelW = maxPanelW;
        if (consoleH < minConsoleH)
            consoleH = minConsoleH;
        if (consoleH > (float)winH * 0.5f)
            consoleH = (float)winH * 0.5f;

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)winW, toolbarH));
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
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

        float contentY = toolbarH;
        float contentH = (float)winH - toolbarH - consoleH;
        if (contentH < minGameH)
            contentH = minGameH;
        float gameW = (float)winW - leftPanelW - rightPanelW;
        if (gameW < minGameW)
        {
            float deficit = minGameW - gameW;
            leftPanelW = std::max(minPanelW, leftPanelW - deficit * 0.5f);
            rightPanelW = std::max(minPanelW, rightPanelW - deficit * 0.5f);
            gameW = (float)winW - leftPanelW - rightPanelW;
        }

        ImGui::SetNextWindowPos(ImVec2(0, contentY));
        ImGui::SetNextWindowSize(ImVec2(leftPanelW, contentH));
        ImGui::Begin("Project", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
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

        ImGui::SetNextWindowPos(ImVec2((float)winW - rightPanelW, contentY));
        ImGui::SetNextWindowSize(ImVec2(rightPanelW, contentH));
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
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

        ImGui::SetNextWindowPos(ImVec2(leftPanelW, contentY));
        ImGui::SetNextWindowSize(ImVec2(gameW, contentH));
        ImGui::Begin("Game View", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("Simulation: %s", simulate ? "Running" : "Stopped");
        Camera2D &cam = engine.camera();
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
                    if (!draggingMove && !draggingScale && ImGui::IsMouseClicked(0))
                    {
                        int pickedId = 0;
                        if (PickEntityAt(engine.scene(), mouseWorldX, mouseWorldY, pickedId))
                            selectedEntityId = pickedId;
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Camera");
        ImGui::InputFloat("Cam X", &cam.x);
        ImGui::InputFloat("Cam Y", &cam.y);
        ImGui::InputFloat("Zoom", &cam.zoom);
        if (cam.zoom < 0.1f)
            cam.zoom = 0.1f;
        if (haveMouseWorld)
            ImGui::Text("Mouse World: %.1f, %.1f", mouseWorldX, mouseWorldY);
        ImGui::End();

        if (sceneTexture && selectedEntityId != 0)
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

                ImDrawList *dl = ImGui::GetForegroundDrawList();
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

                ImVec2 mousePos = ImGui::GetMousePos();
                bool overMove = mousePos.x >= moveMin.x && mousePos.x <= moveMax.x && mousePos.y >= moveMin.y && mousePos.y <= moveMax.y;
                bool overScale = mousePos.x >= scaleMin.x && mousePos.x <= scaleMax.x && mousePos.y >= scaleMin.y && mousePos.y <= scaleMax.y;

                if (gameViewHovered && ImGui::IsMouseClicked(0))
                {
                    if (overMove)
                    {
                        draggingMove = true;
                        draggingScale = false;
                        dragEntityId = selectedEntityId;
                        dragOffsetX = mouseWorldX - selectedEnt->transform.x;
                        dragOffsetY = mouseWorldY - selectedEnt->transform.y;
                    }
                    else if (overScale)
                    {
                        draggingScale = true;
                        draggingMove = false;
                        dragEntityId = selectedEntityId;
                        dragType = btype;
                    }
                }

                if (draggingMove && dragEntityId == selectedEntityId && haveMouseWorld)
                {
                    selectedEnt->transform.x = mouseWorldX - dragOffsetX;
                    selectedEnt->transform.y = mouseWorldY - dragOffsetY;
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

                    if (ImGui::IsMouseReleased(0))
                        draggingScale = false;
                }
            }
        }

        ImGui::SetNextWindowPos(ImVec2(0, (float)winH - consoleH));
        ImGui::SetNextWindowSize(ImVec2((float)winW, consoleH));
        ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped("%s", importStatus.c_str());
        ImGui::End();

        gameViewInputActive = (gameViewHovered || gameViewFocused) && !(draggingMove || draggingScale);

        ImGui::SetNextWindowPos(ImVec2(leftPanelW - 3.0f, contentY));
        ImGui::SetNextWindowSize(ImVec2(6.0f, contentH));
        ImGui::Begin("##split_left", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
        DrawSplitter(true, 6.0f, leftPanelW, minPanelW, (float)winW - rightPanelW - minGameW, 1.0f);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2((float)winW - rightPanelW - 3.0f, contentY));
        ImGui::SetNextWindowSize(ImVec2(6.0f, contentH));
        ImGui::Begin("##split_right", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
        DrawSplitter(true, 6.0f, rightPanelW, minPanelW, (float)winW - leftPanelW - minGameW, -1.0f);
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0, (float)winH - consoleH - 3.0f));
        ImGui::SetNextWindowSize(ImVec2((float)winW, 6.0f));
        ImGui::Begin("##split_bottom", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
        DrawSplitter(false, 6.0f, consoleH, minConsoleH, (float)winH - toolbarH - minGameH, 1.0f);
        ImGui::End();

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
