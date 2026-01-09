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
#include "../Systems/RenderSystem.h"
#include "../Renderer/RenderQueue.h"
#include "Camera2D.h"

class IGame;

class Engine
{
public:
    Engine();
    ~Engine();

    void run(IGame &game);

    Camera2D &camera() { return camera_; }
    const Camera2D &camera() const { return camera_; }

    int screenWidth() const { return width_; }
    int screenHeight() const { return height_; }

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

private:
    bool init();
    void shutdown();
    void processInput();

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
    RenderSystem renderSystem_;
    RenderQueue renderQueue_;
};
