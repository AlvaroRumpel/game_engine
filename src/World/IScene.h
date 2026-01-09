#pragma once

class Engine;

class IScene
{
public:
    virtual ~IScene() = default;

    virtual void onEnter(Engine &engine) {}
    virtual void onExit(Engine &engine) {}
    virtual void onUpdate(Engine &engine, float dt) = 0;
    virtual void onFixedUpdate(Engine &engine, float fixedDt) {}
    virtual void onRenderUI(Engine &engine) {}
};
