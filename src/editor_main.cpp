#define SDL_MAIN_HANDLED
#include "Engine/Engine.h"
#include "Game/SandboxScenes.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
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

    Uint32 lastTicks = SDL_GetTicks();

    while (true)
    {
        engine.beginInputFrame();

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL2_ProcessEvent(&e);
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
