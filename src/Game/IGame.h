#pragma once

class Engine;

class IGame
{
public:
    virtual ~IGame() = default;

    virtual void onInit(Engine &engine) {}
    virtual void onShutdown(Engine &engine) {}

    virtual void onEvent(Engine &engine) {} // opcional (eventos SDL)
    virtual void onUpdate(Engine &engine, float dt) = 0;
    virtual void onRender(Engine &engine) = 0;
    virtual void onFixedUpdate(Engine &engine, float fixedDt) {}
};
