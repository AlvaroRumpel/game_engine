#include "Engine.h"
#include "../Renderer/SDLRenderer.h"

#include <SDL.h>
#include <cstdio>

static Key ToKey(SDL_Keycode k)
{
    switch (k)
    {
    case SDLK_a:
        return Key::A;
    case SDLK_c:
        return Key::C;
    case SDLK_d:
        return Key::D;
    case SDLK_w:
        return Key::W;
    case SDLK_s:
        return Key::S;
    case SDLK_q:
        return Key::Q;
    case SDLK_e:
        return Key::E;
    case SDLK_TAB:
        return Key::Tab;

    case SDLK_LEFT:
        return Key::Left;
    case SDLK_RIGHT:
        return Key::Right;
    case SDLK_UP:
        return Key::Up;
    case SDLK_DOWN:
        return Key::Down;

    case SDLK_ESCAPE:
        return Key::Escape;
    default:
        return Key::Unknown;
    }
}

Engine::Engine() = default;

Engine::~Engine()
{
    shutdown();
}

void Engine::run(std::unique_ptr<IScene> startScene)
{
    if (!start())
        return;

    setScene(std::move(startScene));
    applyPendingScene();

    Uint32 lastTicks = SDL_GetTicks();

    while (running_)
    {
        Uint32 nowTicks = SDL_GetTicks();
        float rawDt = (nowTicks - lastTicks) / 1000.0f;
        lastTicks = nowTicks;

        processInput();
        if (quitRequested_)
            running_ = false;

        tick(rawDt);
        renderWorld(true);
        present();

        SDL_Delay(1);
    }

    stop();
}

bool Engine::start()
{
    if (!init())
        return false;

    running_ = true;
    quitRequested_ = false;
    time_.reset();
    return true;
}

void Engine::stop()
{
    running_ = false;
    if (currentScene_)
        currentScene_->onExit(*this);
    shutdown();
}

void Engine::beginInputFrame()
{
    input_.beginFrame();
}

void Engine::handleEvent(const SDL_Event &e)
{
    if (e.type == SDL_QUIT)
    {
        quitRequested_ = true;
    }

    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
    {
        bool down = (e.type == SDL_KEYDOWN);

        Key key = ToKey(e.key.keysym.sym);
        if (key != Key::Unknown)
        {
            input_.setKeyDown(key, down);
        }

        if (down && key == Key::Escape)
        {
            quitRequested_ = true;
        }
    }

    if (e.type == SDL_MOUSEMOTION)
    {
        input_.setMousePosition(e.motion.x, e.motion.y);
    }

    if (e.type == SDL_MOUSEWHEEL)
    {
        float wheel = (float)e.wheel.y;
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            wheel = -wheel;
        input_.setMouseWheelDelta(wheel);
    }
}

void Engine::finalizeInput()
{
    int mx = 0;
    int my = 0;
    SDL_GetMouseState(&mx, &my);
    input_.setMousePosition(mx, my);
}

void Engine::tick(float dt)
{
    assets().updateHotReload();

    // Frame timing
    time_.beginFrame(dt);
    applyPendingScene();

    // Fixed updates (0..N por frame)
    int steps = 0;
    while (time_.accumulator() >= time_.fixedDelta() &&
           steps < time_.maxFixedStepsPerFrame())
    {
        if (currentScene_)
            currentScene_->onFixedUpdate(*this, time_.fixedDelta());
        physicsSystem_.step(*this, scene_, time_.fixedDelta(), currentScene_.get());
        time_.consumeFixedStep();
        steps++;
    }

    // Update variavel (1x por frame)
    if (currentScene_)
        currentScene_->onUpdate(*this, time_.deltaTime());
}

void Engine::renderWorld(bool includeSceneUI)
{
    renderWorld(includeSceneUI, width_, height_);
}

void Engine::renderWorld(bool includeSceneUI, int viewW, int viewH)
{
    // Render
    renderer_->beginFrame();

    commandBuffer_.nextFrame(time_.frameCount());
    commandBuffer_.clear();

    // Tilemap + RenderSystem coleta comandos do mundo
    tilemapSystem_.render(*this, scene_);
    renderSystem_.render(*this, scene_);
    if (physicsDebugDraw_)
        physicsSystem_.debugRender(*this, scene_);

    if (includeSceneUI && currentScene_)
        currentScene_->onRenderUI(*this);

    commandBuffer_.finalize();

    if (viewW <= 0)
        viewW = width_;
    if (viewH <= 0)
        viewH = height_;
    renderer_->setCamera(camera_, viewW, viewH);
    // Executa o command buffer (desenha de fato)
    renderer_->submit(commandBuffer_);

    static int counter = 0;
    counter++;
    if (counter % 120 == 0)
    {
        const auto &s = commandBuffer_.stats();
        std::printf("[RenderStats] cmds=%u rect=%u sprite=%u binds~=%u\n",
                    s.commandsSubmitted, s.rectDraws, s.spriteDraws, s.textureBindsEstimated);
    }
}

void Engine::present()
{
    renderer_->endFrame();
}
bool Engine::init()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::printf("SDL_Init error: %s\n", SDL_GetError());
        return false;
    }

    window_ = SDL_CreateWindow(
        "Game Engine",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width_,
        height_,
        SDL_WINDOW_SHOWN);

    if (!window_)
    {
        std::printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    sdlRenderer_ = SDL_CreateRenderer(
        window_,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!sdlRenderer_)
    {
        std::printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    // Cria o renderer “real” (backend SDL)
    auto sdlBackend = std::make_unique<SDLRenderer>(sdlRenderer_);
    backendRenderer_ = sdlBackend.get();
    renderer_ = std::move(sdlBackend);

    assets_ = std::make_unique<AssetManager>(*backendRenderer_);
    assets_->loadManifest("assets/manifest.txt");

    input_.setAxisMapping("MoveX", AxisMapping{
                                       /*positive*/ {Key::D, Key::Right},
                                       /*negative*/ {Key::A, Key::Left}});

    // Axis: MoveY = (W/Up) negativo, (S/Down) positivo
    input_.setAxisMapping("MoveY", AxisMapping{
                                       /*positive*/ {Key::S, Key::Down},
                                       /*negative*/ {Key::W, Key::Up}});

    input_.setActionBinding("ZoomIn", ActionBinding{
                                          /*keys*/ {Key::E},
                                          /*wheelDir*/ 1});
    input_.setActionBinding("ZoomOut", ActionBinding{
                                           /*keys*/ {Key::Q},
                                           /*wheelDir*/ -1});
    input_.setActionBinding("ToggleDebug", ActionBinding{
                                               /*keys*/ {Key::Tab}});
    input_.setActionBinding("ToggleCollision", ActionBinding{
                                                  /*keys*/ {Key::C}});

    return true;
}

void Engine::shutdown()
{
    assets_.reset();
    renderer_.reset();
    backendRenderer_ = nullptr;

    if (sdlRenderer_)
    {
        SDL_DestroyRenderer(sdlRenderer_);
        sdlRenderer_ = nullptr;
    }
    if (window_)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void Engine::processInput()
{
    beginInputFrame();

    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        handleEvent(e);
    }

    finalizeInput();
}

void Engine::setScene(std::unique_ptr<IScene> nextScene)
{
    pendingScene_ = std::move(nextScene);
}

void Engine::applyPendingScene()
{
    if (!pendingScene_)
        return;

    if (currentScene_)
        currentScene_->onExit(*this);

    scene_.clear();
    currentScene_ = std::move(pendingScene_);
    physicsSystem_.reset();
    if (currentScene_)
        currentScene_->onEnter(*this);
}

Entity &Engine::createEntity()
{
    Entity e;
    e.id = nextEntityId_++;
    entities_.push_back(e);
    return entities_.back();
}

Entity *Engine::findEntity(int id)
{
    for (auto &e : entities_)
    {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}
