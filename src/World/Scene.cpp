#include "Scene.h"

Entity &Scene::createEntity()
{
    Entity e;
    e.id = nextEntityId_++;
    entities_.push_back(e);
    return entities_.back();
}

Entity *Scene::findEntity(int id)
{
    for (auto &e : entities_)
    {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

void Scene::clear()
{
    entities_.clear();
    nextEntityId_ = 1;
}
