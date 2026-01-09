#pragma once

class Engine;
class Scene;

class RenderSystem
{
public:
    void render(Engine &engine, const Scene &scene);
};
