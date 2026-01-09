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

    virtual void onCollisionEnter(Engine &engine, int aId, int bId) {}
    virtual void onCollisionStay(Engine &engine, int aId, int bId) {}
    virtual void onCollisionExit(Engine &engine, int aId, int bId) {}
};
