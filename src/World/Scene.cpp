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

Tilemap &Scene::createTilemap(int width, int height, int tileSize)
{
    Tilemap map;
    map.width = width;
    map.height = height;
    map.tileSize = tileSize;
    map.tiles.resize(width * height, -1);
    tilemaps_.push_back(std::move(map));
    return tilemaps_.back();
}

void Scene::clear()
{
    entities_.clear();
    nextEntityId_ = 1;
    tilemaps_.clear();
}
