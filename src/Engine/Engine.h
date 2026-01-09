#pragma once
#include <vector>
#include <memory>

struct SDL_Window;
struct SDL_Renderer;

#include "../Renderer/Renderer.h"
#include "../World/Entity.h"
#include "../Input/Input.h"
#include "../Time/Time.h"
#include "../Assets/AssetManager.h"
#include "../World/Scene.h"
#include "../World/IScene.h"
#include "../Systems/RenderSystem.h"
#include "../Systems/TilemapSystem.h"
#include "../Systems/PhysicsSystem.h"
#include "../Renderer/RenderQueue.h"
#include "Camera2D.h"

class Engine
{
public:
    Engine();
    ~Engine();

    void run(std::unique_ptr<IScene> startScene);
    void setScene(std::unique_ptr<IScene> nextScene);

    Camera2D &camera() { return camera_; }
    const Camera2D &camera() const { return camera_; }

    int screenWidth() const { return width_; }
    int screenHeight() const { return height_; }
    void screenToWorld(float sx, float sy, float &wx, float &wy) const
    {
        wx = (sx - (width_ * 0.5f)) / camera_.zoom + camera_.x;
        wy = (sy - (height_ * 0.5f)) / camera_.zoom + camera_.y;
    }

    // World
    Entity &createEntity();
    Entity *findEntity(int id);
    const std::vector<Entity> &entities() const { return entities_; }

    RenderQueue &renderQueue() { return renderQueue_; }

    // Renderer API
    Renderer &renderer() { return *renderer_; }

    Input &input() { return input_; }
    const Input &input() const { return input_; }

    Time &time() { return time_; }
    const Time &time() const { return time_; }

    AssetManager &assets() { return *assets_; }

    Scene &scene() { return scene_; }
    const Scene &scene() const { return scene_; }

    PhysicsSystem &physics() { return physicsSystem_; }
    const PhysicsSystem &physics() const { return physicsSystem_; }
    const PhysicsStats &physicsStats() const { return physicsSystem_.stats(); }
    bool physicsDebugDraw() const { return physicsDebugDraw_; }
    void setPhysicsDebugDraw(bool enabled) { physicsDebugDraw_ = enabled; }

private:
    bool init();
    void shutdown();
    void processInput();
    void applyPendingScene();

private:
    bool running_ = false;

    SDL_Window *window_ = nullptr;
    SDL_Renderer *sdlRenderer_ = nullptr;

    std::unique_ptr<Renderer> renderer_; // novo (abstração)

    Camera2D camera_;

    int width_ = 800;
    int height_ = 600;

    bool quitRequested_ = false;
    Input input_;

    std::vector<Entity> entities_;
    int nextEntityId_ = 1;

    Time time_;

    std::unique_ptr<AssetManager> assets_;
    SDLRenderer *backendRenderer_ = nullptr;

    Scene scene_;
    std::unique_ptr<IScene> currentScene_;
    std::unique_ptr<IScene> pendingScene_;
    TilemapSystem tilemapSystem_;
    PhysicsSystem physicsSystem_;
    RenderSystem renderSystem_;
    RenderQueue renderQueue_;
    bool physicsDebugDraw_ = false;
};
