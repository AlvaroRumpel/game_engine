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
    if (!init())
        return;

    running_ = true;
    quitRequested_ = false;

    currentScene_ = std::move(startScene);
    if (currentScene_)
    {
        scene_.clear();
        currentScene_->onEnter(*this);
    }

    Uint32 lastTicks = SDL_GetTicks();
    time_.reset();

    while (running_)
    {
        Uint32 nowTicks = SDL_GetTicks();
        float rawDt = (nowTicks - lastTicks) / 1000.0f;
        lastTicks = nowTicks;

        processInput();
        assets().updateHotReload();
        if (quitRequested_)
            running_ = false;

        // Frame timing
        time_.beginFrame(rawDt);
        applyPendingScene();

        // Fixed updates (0..N por frame)
        int steps = 0;
        while (time_.accumulator() >= time_.fixedDelta() &&
               steps < time_.maxFixedStepsPerFrame())
        {
            if (currentScene_)
                currentScene_->onFixedUpdate(*this, time_.fixedDelta());
            time_.consumeFixedStep();
            steps++;
        }

        // Update variavel (1x por frame)
        if (currentScene_)
            currentScene_->onUpdate(*this, time_.deltaTime());

        // Render
        renderer_->beginFrame();

        renderQueue_.nextFrame(time_.frameCount());

        renderQueue_.clear();

        // RenderSystem coleta comandos do mundo
        renderSystem_.render(*this, scene_);

        // Scene pode adicionar debug/UI tambem
        if (currentScene_)
            currentScene_->onRenderUI(*this);

        renderer_->setCamera(camera_, width_, height_);
        // Executa a fila (desenha de fato)
        renderQueue_.flush(*renderer_);
        static int counter = 0;
        counter++;
        if (counter % 120 == 0)
        {
            const auto &s = renderQueue_.stats();
            std::printf("[RenderStats] cmds=%u rect=%u sprite=%u binds~=%u\n",
                        s.commandsSubmitted, s.rectDraws, s.spriteDraws, s.textureBindsEstimated);
        }

        renderer_->endFrame();

        SDL_Delay(1);
    }

    if (currentScene_)
        currentScene_->onExit(*this);
    shutdown();
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
    input_.beginFrame();

    SDL_Event e;
    while (SDL_PollEvent(&e))
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

    int mx = 0;
    int my = 0;
    SDL_GetMouseState(&mx, &my);
    input_.setMousePosition(mx, my);
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
